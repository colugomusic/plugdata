#pragma once

#include "Utility/ZoomableDragAndDropContainer.h"

class TabBarButtonComponent;
class TabComponent : public Component, public DragAndDropTarget, public AsyncUpdater
{
public:
    TabComponent(PluginEditor* editor);
    
    Canvas* newPatch();
    
    Canvas* openPatch(const URL& path);
    Canvas* openPatch(const String& patchContent);
    Canvas* openPatch(pd::Patch::Ptr existingPatch);
    void openPatch();
    
    void renderArea(NVGcontext* nvg, Rectangle<int> bounds);

    void nextTab();
    void previousTab();

    void closeTab(Canvas* cnv);
    void showTab(Canvas* cnv, int splitIndex = 0);
    void setActiveSplit(Canvas* cnv);
    
    void closeAllTabs();

    Canvas* getCurrentCanvas();
    Canvas* getCanvasAtScreenPosition(Point<int> screenPosition);
    
    Array<Canvas*> getCanvases();
    Array<Canvas*> getVisibleCanvases();
    
private:
    
    void handleAsyncUpdate() override;
    
    void resized() override;
    void parentSizeChanged() override;
    
    void saveTabPositions();
    void closeEmptySplits();

    bool isInterestedInDragSource(SourceDetails const& dragSourceDetails) override;
    void itemDropped(SourceDetails const& dragSourceDetails) override;
    void itemDragEnter(SourceDetails const& dragSourceDetails) override;
    void itemDragExit(SourceDetails const& dragSourceDetails) override;
    void itemDragMove(SourceDetails const& dragSourceDetails) override;
    
    void mouseDown(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseMove(const MouseEvent& e) override;
    
    void showHiddenTabsMenu(int splitIndex);
    
    class TabBarButtonComponent : public Component
    {
        
        struct TabDragConstrainer : public ComponentBoundsConstrainer
        {
            TabDragConstrainer(TabComponent* parent) : parent(parent)
            {
                
            }
            void checkBounds (Rectangle<int>& bounds, const Rectangle<int>&, const Rectangle<int>& limits, bool, bool, bool, bool) override
            {
                bounds = bounds.withPosition(std::clamp(bounds.getX(), 30, parent->getWidth() - bounds.getWidth()), 0);
            }
            
            TabComponent* parent;
        };
     
        class CloseTabButton : public SmallIconButton {

            using SmallIconButton::SmallIconButton;

            void paint(Graphics& g) override
            {
                auto font = Fonts::getIconFont().withHeight(12);
                g.setFont(font);

                if (!isEnabled()) {
                    g.setColour(Colours::grey);
                } else if (getToggleState()) {
                    g.setColour(findColour(PlugDataColour::toolbarActiveColourId));
                } else if (isMouseOver()) {
                    g.setColour(findColour(PlugDataColour::toolbarTextColourId).brighter(0.8f));
                } else {
                    g.setColour(findColour(PlugDataColour::toolbarTextColourId));
                }

                int const yIndent = jmin(4, proportionOfHeight(0.3f));
                int const cornerSize = jmin(getHeight(), getWidth()) / 2;

                int const fontHeight = roundToInt(font.getHeight() * 0.6f);
                int const leftIndent = jmin(fontHeight, 2 + cornerSize / (isConnectedOnLeft() ? 4 : 2));
                int const rightIndent = jmin(fontHeight, 2 + cornerSize / (isConnectedOnRight() ? 4 : 2));
                int const textWidth = getWidth() - leftIndent - rightIndent;

                if (textWidth > 0)
                    g.drawFittedText(getButtonText(), leftIndent, yIndent, textWidth, getHeight() - yIndent * 2, Justification::centred, 2);
            }
        };
        
    public:
        TabBarButtonComponent(Canvas* cnv, TabComponent* parent) : cnv(cnv), parent(parent), tabDragConstrainer(parent)
        {
            closeButton.onClick = [cnv = SafePointer(cnv), parent](){
                if(cnv) parent->closeTab(cnv);
            };
            closeButton.addMouseListener(this, false);
            closeButton.setSize(28, 28);
            addAndMakeVisible(closeButton);
            setRepaintsOnMouseActivity(true);
        }
        
        void paint(Graphics& g) override
        {
            auto mouseOver = isMouseOver();
            auto active = isActive();
            if (active) {
                g.setColour(findColour(PlugDataColour::activeTabBackgroundColourId));
            } else if (mouseOver) {
                g.setColour(findColour(PlugDataColour::activeTabBackgroundColourId).interpolatedWith(findColour(PlugDataColour::toolbarBackgroundColourId), 0.4f));
            } else {
                g.setColour(findColour(PlugDataColour::toolbarBackgroundColourId));
            }

            PlugDataLook::fillSmoothedRectangle(g, getLocalBounds().toFloat().reduced(4.5f), Corners::defaultCornerRadius);

            auto area = getLocalBounds().reduced(4, 1).toFloat();

            // Use a gradient to make it fade out when it gets near to the close button
            auto fadeX = (mouseOver || active) ? area.getRight() - 25 : area.getRight() - 8;
            auto textColour = findColour(PlugDataColour::toolbarTextColourId);
            g.setGradientFill(ColourGradient(textColour, fadeX - 18, area.getY(), Colours::transparentBlack, fadeX, area.getY(), false));
            
            auto text = cnv->patch.getTitle() + (cnv->patch.isDirty() ? String("*") : String());
            
            g.setFont(Fonts::getCurrentFont().withHeight(14.0f));
            g.drawText(text, area.reduced(4, 0), Justification::centred, false);
        }
        
        void resized() override
        {
            closeButton.setCentrePosition(getLocalBounds().getCentre().withX(getWidth() - 15).translated(0, 1));
        }
        
        ScaledImage generateTabBarButtonImage()
        {
            auto scale = 2.0f;
            // we calculate the best size for the tab DnD image
            auto text = cnv->patch.getTitle();
            Font font(Fonts::getCurrentFont());
            auto length = font.getStringWidth(text) + 32;
            auto const boundsOffset = 10;

            // we need to expand the bounds, but reset the position to top left
            // then we offset the mouse drag by the same amount
            // this is to allow area for the shadow to render correctly
            auto textBounds = Rectangle<int>(0, 0, length, 28);
            auto bounds = textBounds.expanded(boundsOffset).withZeroOrigin();
            auto image = Image(Image::PixelFormat::ARGB, bounds.getWidth() * scale, bounds.getHeight() * scale, true);
            auto g = Graphics(image);
            g.addTransform(AffineTransform::scale(scale));
            Path path;
            path.addRoundedRectangle(bounds.reduced(10), 5.0f);
            StackShadow::renderDropShadow(g, path, Colour(0, 0, 0).withAlpha(0.3f), 7, { 0, 1 }, scale);
            g.setOpacity(1.0f);
            
            g.setColour(findColour(PlugDataColour::toolbarBackgroundColourId));
            PlugDataLook::fillSmoothedRectangle(g, textBounds.withPosition(10, 10).toFloat(), Corners::defaultCornerRadius);

            g.setColour(findColour(PlugDataColour::toolbarTextColourId));

            g.setFont(font);
            g.drawText(text, textBounds.withPosition(10, 10), Justification::centred, false);

            return ScaledImage(image, scale);
        }
        
        void mouseDown(const MouseEvent& e) override
        {
            toFront(false);
            parent->showTab(cnv, parent->tabbars[1].contains(this));
            dragger.startDraggingComponent(this, e);
        }
        
        void mouseDrag(const MouseEvent& e) override
        {
            if (e.getDistanceFromDragStart() > 10 && !isDragging) {
                isDragging = true;
                auto dragContainer = ZoomableDragAndDropContainer::findParentDragContainerFor(this);
                
                tabImage = generateTabBarButtonImage();
                dragContainer->startDragging(1, this, tabImage, tabImage, true, nullptr);
            }
            else if(parent->draggingOverTabbar) {
                dragger.dragComponent(this, e, &tabDragConstrainer);
            }
        }
        
        void mouseUp(const MouseEvent& e) override
        {
            isDragging = false;
            setVisible(true);
            parent->resized(); // call resized so the dropped tab will animate into its correct position
        }
        
        bool isActive() const
        {
            return cnv && (parent->splits[0] == cnv || parent->splits[1] == cnv);
        }
        
        // close button, etc.
        SafePointer<Canvas> cnv;
        TabComponent* parent;
        ScaledImage tabImage;
        bool isDragging = false;
        ComponentDragger dragger;
        TabDragConstrainer tabDragConstrainer;
        
        CloseTabButton closeButton = CloseTabButton(Icons::Clear);
    };
    
    std::array<MainToolbarButton, 2> newTabButtons = {MainToolbarButton(Icons::Add), MainToolbarButton(Icons::Add)};
    std::array<MainToolbarButton, 2> tabOverflowButtons = {MainToolbarButton(Icons::ThinDown), MainToolbarButton(Icons::ThinDown)};
    
    std::array<OwnedArray<TabBarButtonComponent>, 2> tabbars;
    std::array<SafePointer<Canvas>, 2> splits = {nullptr, nullptr};
    
    bool draggingOverTabbar = false;
    bool draggingSplitResizer = false;
    Rectangle<int> splitDropBounds;
    
    int splitSize = 0;
    int activeSplitIndex = 0;
    uint32 lastMouseTime = 0;
    
    OwnedArray<Canvas, CriticalSection> canvases;

    PluginEditor* editor;
    PluginProcessor* pd;
};
