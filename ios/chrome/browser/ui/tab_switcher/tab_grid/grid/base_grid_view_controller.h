// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import <set>

#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_theme.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"

@protocol BaseGridMediatorItemProvider;
@class BaseGridViewController;
@protocol GridEmptyView;
@class GridItemIdentifier;
@protocol GridViewControllerMutator;
@class LegacyGridTransitionLayout;
@protocol PriceCardDataSource;
@protocol SuggestedActionsDelegate;
@protocol TabContextMenuProvider;
@protocol TabCollectionDragDropHandler;
@class TabGridTransitionItem;

namespace web {
class WebStateID;
}  // namespace web

// Protocol used to relay relevant user interactions from a grid UI.
@protocol GridViewControllerDelegate

// Tells the delegate that the item with `itemID` was selected in
// `gridViewController`.
- (void)gridViewController:(BaseGridViewController*)gridViewController
       didSelectItemWithID:(web::WebStateID)itemID;
// Tells the delegate that the item with `itemID` was closed in
// `gridViewController`.
- (void)gridViewController:(BaseGridViewController*)gridViewController
        didCloseItemWithID:(web::WebStateID)itemID;
// Tells the delegate that the item with `itemID` was moved to
// `destinationIndex`.
- (void)gridViewController:(BaseGridViewController*)gridViewController
         didMoveItemWithID:(web::WebStateID)itemID
                   toIndex:(NSUInteger)destinationIndex;
// Tells the delegate that the the number of items in `gridViewController`
// changed to `count`.
- (void)gridViewController:(BaseGridViewController*)gridViewController
        didChangeItemCount:(NSUInteger)count;
// Tells the delegate that the item with `itemID` was removed.
- (void)gridViewController:(BaseGridViewController*)gridViewController
       didRemoveItemWIthID:(web::WebStateID)itemID;

// Tells the delegate that the visibility of the last item of the grid changed.
- (void)didChangeLastItemVisibilityInGridViewController:
    (BaseGridViewController*)gridViewController;

// Tells the delegate that the grid view controller's scroll view will begin
// dragging.
- (void)gridViewControllerWillBeginDragging:
    (BaseGridViewController*)gridViewController;
// Tells the delegate that the grid view controller cells will begin dragging.
- (void)gridViewControllerDragSessionWillBegin:
    (BaseGridViewController*)gridViewController;
// Tells the delegate that the grid view controller cells did end dragging.
- (void)gridViewControllerDragSessionDidEnd:
    (BaseGridViewController*)gridViewController;
// Tells the delegate that the grid view controller did scroll.
- (void)gridViewControllerScrollViewDidScroll:
    (BaseGridViewController*)gridViewController;

// Tells the delegate that a drop animation will begin.
- (void)gridViewControllerDropAnimationWillBegin:
    (BaseGridViewController*)gridViewController;
// Tells the delegate that a drop animation did end.
- (void)gridViewControllerDropAnimationDidEnd:
    (BaseGridViewController*)gridViewController;

// Tells the delegate that the inactive tabs button was tapped in
// `gridViewController`, i.e., there was an intention to show inactive tabs (in
// TabGridModeNormal).
- (void)didTapInactiveTabsButtonInGridViewController:
    (BaseGridViewController*)gridViewController;

// Tells the delegate that the inactive tabs settings link was tapped in
// `gridViewController`, i.e., there was an intention to show inactive tabs
// settings (in TabGridModeInactive).
- (void)didTapInactiveTabsSettingsLinkInGridViewController:
    (BaseGridViewController*)gridViewController;

@end

// A view controller that contains a grid of items.
@interface BaseGridViewController : UIViewController <TabCollectionConsumer>
// Whether the grid is scrolled to the top.
@property(nonatomic, readonly, getter=isScrolledToTop) BOOL scrolledToTop;
// Whether the grid is scrolled to the bottom.
@property(nonatomic, readonly, getter=isScrolledToBottom) BOOL scrolledToBottom;
// The view that is shown when there are no items.
@property(nonatomic, strong) UIView<GridEmptyView>* emptyStateView;
// Returns YES if the grid has no items.
@property(nonatomic, readonly, getter=isGridEmpty) BOOL gridEmpty;
// Returns YES if contained grids have no items.
@property(nonatomic, readonly, getter=isContainedGridEmpty)
    BOOL containedGridEmpty;
// The visual look of the grid.
@property(nonatomic, assign) GridTheme theme;
// The current mode for the grid.
@property(nonatomic, assign) TabGridMode mode;
// The current search text to use for filtering results when the search mode is
// active.
@property(nonatomic, copy) NSString* searchText;
// Delegate for search results suggested actions.
@property(nonatomic, weak) id<SuggestedActionsDelegate>
    suggestedActionsDelegate;
// Delegate is informed of user interactions in the grid UI.
@property(nonatomic, weak) id<GridViewControllerDelegate> delegate;
// Mutator is informed when the model should be updated after user interaction.
@property(nonatomic, weak) id<GridViewControllerMutator> mutator;
// Provider of the grid.
@property(nonatomic, weak) id<BaseGridMediatorItemProvider> gridProvider;
// Handles drag and drop interactions that involved the model layer.
@property(nonatomic, weak) id<TabCollectionDragDropHandler> dragDropHandler;
// Tracks if a drop animation is in progress.
@property(nonatomic, assign) BOOL dropAnimationInProgress;
// Data source for acquiring data to power PriceCardView
@property(nonatomic, weak) id<PriceCardDataSource> priceCardDataSource;
// YES if the selected cell is visible in the grid.
@property(nonatomic, readonly, getter=isSelectedCellVisible)
    BOOL selectedCellVisible;
// Provider of context menu configurations for the tabs in the grid.
@property(nonatomic, weak) id<TabContextMenuProvider> menuProvider;
// Opacity of grid cells that are not the selected tab.
@property(nonatomic, assign) CGFloat notSelectedTabCellOpacity;
// The insets to set on the collection view.
// Setting content insets on the collection view is a workaround. Indeed,
// ideally, grids would just honor the safe area. But since the 3 panes are part
// of a scrollview, they don't all get the correct information when being laid
// out. To that end, contentInsets are manually added.
@property(nonatomic, assign) UIEdgeInsets contentInsets;

// Returns the layout of the grid for use in an animated transition.
- (LegacyGridTransitionLayout*)transitionLayout;

// Returns TabGridTransitionItem for the active cell.
- (TabGridTransitionItem*)transitionItemForActiveCell;

// Notifies the ViewController that its content might soon be displayed.
- (void)prepareForAppearance;
// Notifies the ViewController that its content is being displayed.
- (void)contentWillAppearAnimated:(BOOL)animated;
- (void)contentDidAppear;
// Notifies the ViewController that its content is being hidden.
- (void)contentWillDisappear;

// Notifies the grid that it is about to be dismissed.
- (void)prepareForDismissal;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_VIEW_CONTROLLER_H_
