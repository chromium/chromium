// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_PINNED_TABS_PINNED_TABS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_PINNED_TABS_PINNED_TABS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/pinned_tab_collection_consumer.h"

@class LegacyGridTransitionLayout;
@class PinnedTabsViewController;
@protocol TabCollectionDragDropHandler;
@protocol TabContextMenuProvider;
@class TabGridTransitionItem;

namespace web {
class WebStateID;
}  // namespace web

// Protocol used to relay relevant user interactions from the
// PinnedTabsViewController.
@protocol PinnedTabsViewControllerDelegate

// Tells the delegate that the item with `itemID` in `pinnedTabsViewController`
// was selected.
- (void)pinnedTabsViewController:
            (PinnedTabsViewController*)pinnedTabsViewController
             didSelectItemWithID:(web::WebStateID)itemID;

// Tells the delegate that the item with `itemID` was moved.
- (void)pinnedTabsViewControllerDidMoveItem:
    (PinnedTabsViewController*)pinnedTabsViewController;

// Tells the delegate that the item with `itemID` was removed.
- (void)pinnedTabsViewController:(PinnedTabsViewController*)gridViewController
             didRemoveItemWithID:(web::WebStateID)itemID;

// Tells the delegate that the `pinnedTabsViewController` visibility has
// changed.
- (void)pinnedTabsViewControllerVisibilityDidChange:
    (PinnedTabsViewController*)pinnedTabsViewController;

// Tells the delegate that a drop animation will begin.
- (void)pinnedViewControllerDropAnimationWillBegin:
    (PinnedTabsViewController*)pinnedTabsViewController;

// Tells the delegate that a drop animation did end.
- (void)pinnedViewControllerDropAnimationDidEnd:
    (PinnedTabsViewController*)pinnedTabsViewController;

// Tells the delegate that a drag session did end.
- (void)pinnedViewControllerDragSessionWillBegin:
    (PinnedTabsViewController*)pinnedTabsViewController;

// Tells the delegate that a drag session did end.
- (void)pinnedViewControllerDragSessionDidEnd:
    (PinnedTabsViewController*)pinnedTabsViewController;

// Tells the delegate that a context menu has been requested.
- (void)pinnedViewControllerDidRequestContextMenu:
    (PinnedTabsViewController*)pinnedTabsViewController;

@end

// UICollectionViewController used to display pinned tabs.
@interface PinnedTabsViewController
    : UICollectionViewController <PinnedTabCollectionConsumer>

// Delegate used to to relay relevant user interactions.
@property(nonatomic, weak) id<PinnedTabsViewControllerDelegate> delegate;

// Provides context menus.
@property(nonatomic, weak) id<TabContextMenuProvider> menuProvider;

// Handles drag and drop interactions that involved the model layer.
@property(nonatomic, weak) id<TabCollectionDragDropHandler> dragDropHandler;

// Tracks if a drop animation is in progress.
@property(nonatomic, assign) BOOL dropAnimationInProgress;

// Tracks the visibility of the view.
@property(nonatomic, readonly) BOOL visible;

// Returns YES if the collection has no items.
@property(nonatomic, readonly, getter=isCollectionEmpty) BOOL collectionEmpty;

// YES if the selected cell is visible in the Pinned Tabs collection.
@property(nonatomic, readonly, getter=isSelectedCellVisible)
    BOOL selectedCellVisible;

// Updates the view when starting or ending a drag action.
- (void)dragSessionEnabled:(BOOL)enabled;

// Makes the pinned tabs view available. The pinned view should only be
// available when the regular tabs grid is displayed.
- (void)pinnedTabsAvailable:(BOOL)available;

// Updates the view when the drop animation did end.
- (void)dropAnimationDidEnd;

// Returns the layout of the pinned tabs to be used in an animated transition.
- (LegacyGridTransitionLayout*)transitionLayout;

// Returns TabGridTransitionItem for the active cell.
- (TabGridTransitionItem*)transitionItemForActiveCell;

// Returns whether there is a selected cell in the collection.
- (BOOL)hasSelectedCell;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (instancetype)initWithCollectionViewLayout:(UICollectionViewLayout*)layout
    NS_UNAVAILABLE;

// Notifies the ViewController that its content is being displayed or hidden.
- (void)contentWillAppearAnimated:(BOOL)animated;
- (void)contentWillDisappear;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_PINNED_TABS_PINNED_TABS_VIEW_CONTROLLER_H_
