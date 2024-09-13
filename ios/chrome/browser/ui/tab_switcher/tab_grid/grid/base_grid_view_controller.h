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
@protocol TabGridCommands;
@protocol TabGroupConfirmationCommands;
@class TabGridTransitionItem;
class TabGroup;

namespace web {
class WebStateID;
}  // namespace web

// Protocol used to relay relevant user interactions from a grid UI.
@protocol GridViewControllerDelegate

// Tells the delegate that the item with `itemID` was selected in
// `gridViewController`.
- (void)gridViewController:(BaseGridViewController*)gridViewController
       didSelectItemWithID:(web::WebStateID)itemID;
// Tells the delegate that the `group` was selected in `gridViewController`.
- (void)gridViewController:(BaseGridViewController*)gridViewController
            didSelectGroup:(const TabGroup*)group;
// Tells the delegate that the item with `itemID` was closed in
// `gridViewController`.
- (void)gridViewController:(BaseGridViewController*)gridViewController
        didCloseItemWithID:(web::WebStateID)itemID;
// Tells the delegate that an item was moved.
- (void)gridViewControllerDidMoveItem:
    (BaseGridViewController*)gridViewController;
// Tells the delegate that the item with `itemID` was removed.
- (void)gridViewController:(BaseGridViewController*)gridViewController
       didRemoveItemWithID:(web::WebStateID)itemID;

// Tells the delegate that the `gridViewController` will begin dragging a tab.
- (void)gridViewControllerDragSessionWillBeginForTab:
    (BaseGridViewController*)gridViewController;
// Tells the delegate that the `gridViewController` will begin dragging a tab
// group.
- (void)gridViewControllerDragSessionWillBeginForTabGroup:
    (BaseGridViewController*)gridViewController;
// Tells the delegate that the `gridViewController` cells did end dragging.
- (void)gridViewControllerDragSessionDidEnd:
    (BaseGridViewController*)gridViewController;
// Tells the delegate that the `gridViewController` did scroll.
- (void)gridViewControllerScrollViewDidScroll:
    (BaseGridViewController*)gridViewController;

// Tells the delegate that a drop animation will begin.
- (void)gridViewControllerDropAnimationWillBegin:
    (BaseGridViewController*)gridViewController;
// Tells the delegate that a drop animation did end.
- (void)gridViewControllerDropAnimationDidEnd:
    (BaseGridViewController*)gridViewController;
// Tells the delegate that a drop session did enter.
- (void)gridViewControllerDropSessionDidEnter:
    (BaseGridViewController*)gridViewController;
// Tells the delegate that a drop session did exit.
- (void)gridViewControllerDropSessionDidExit:
    (BaseGridViewController*)gridViewController;

// Tells the delegate that the inactive tabs button was tapped in
// `gridViewController`, i.e., there was an intention to show inactive tabs (in
// TabGridMode::kNormal).
- (void)didTapInactiveTabsButtonInGridViewController:
    (BaseGridViewController*)gridViewController;

// Tells the delegate that the inactive tabs settings link was tapped in
// `gridViewController`, i.e., there was an intention to show inactive tabs
// settings (in the Inactive grid).
- (void)didTapInactiveTabsSettingsLinkInGridViewController:
    (BaseGridViewController*)gridViewController;

// Tells the delegate that a context menu has been requested.
- (void)gridViewControllerDidRequestContextMenu:
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
// The current search text to use for filtering results when the search mode is
// active.
@property(nonatomic, copy) NSString* searchText;
// Delegate for search results suggested actions. Only available in regular.
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
// A Boolean value that controls whether the scroll-to-top gesture is enabled.
// It is a wrapper around the inner `collectionView.scrollsToTop` property.
// The default value of this property is YES.
@property(nonatomic, assign, getter=isGridScrollsToTopEnabled)
    BOOL gridScrollsToTopEnabled;
// Tab Grid handler.
@property(nonatomic, weak) id<TabGridCommands> tabGridHandler;
// Handler for tab group confirmation commands.
@property(nonatomic, weak) id<TabGroupConfirmationCommands>
    tabGroupConfirmationHandler;

// Returns the layout of the grid for use in an animated transition.
- (LegacyGridTransitionLayout*)transitionLayout;

// Returns TabGridTransitionItem for the active cell.
- (TabGridTransitionItem*)transitionItemForActiveCell;

// Notifies the ViewController that its content is being displayed.
- (void)contentWillAppearAnimated:(BOOL)animated;

// Notifies the grid that it is about to be dismissed.
- (void)prepareForDismissal;

// Moves the visible cells such as their center is in `center` (expressed in
// self.view's coordinates) and apply `scale`. `translationCompletion` is used
// to start the translation from a percentage of the total distance.
- (void)centerVisibleCellsToPoint:(CGPoint)center
            translationCompletion:(CGFloat)translationCompletion
                        withScale:(CGFloat)scale;
// Resets the move and scale done by the method just above.
- (void)resetVisibleCellsCenterAndScale;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_VIEW_CONTROLLER_H_
