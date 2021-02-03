// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/gestures/layout_switcher_provider.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_animatee.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/grid_transition_animation_layout_providing.h"

@protocol ApplicationCommands;
@protocol IncognitoReauthCommands;
@protocol IncognitoReauthConsumer;
@protocol GridConsumer;
@protocol GridCommands;
@protocol GridDragDropHandler;
@protocol GridImageDataSource;
@protocol PopupMenuCommands;
@protocol RecentTabsConsumer;
@class RecentTabsTableViewController;
@class TabGridViewController;

// Delegate protocol for an object that can handle presenting ("opening") tabs
// from the tab grid.
@protocol TabPresentationDelegate <NSObject>
// Show the active tab in |page|, presented on top of the tab grid.  The
// omnibox will be focused after the animation if |focusOmnibox| is YES. If
// |closeTabGrid| is NO, then the tab grid will not be closed, and the active
// tab will simply be displayed in its current position.
// This last parameter is used for the thumb strip, where the
// BVCContainerViewController is never dismissed.
- (void)showActiveTabInPage:(TabGridPage)page
               focusOmnibox:(BOOL)focusOmnibox
               closeTabGrid:(BOOL)closeTabGrid;
@end

@protocol TabGridViewControllerDelegate <NSObject>

// Asks the delegate for the page that should currently be active.
- (TabGridPage)activePageForTabGridViewController:
    (TabGridViewController*)tabGridViewController;

// Notifies the delegate that the tab grid was dismissed via the
// ViewRevealingAnimatee.
- (void)tabGridViewControllerDidDismiss:
    (TabGridViewController*)tabGridViewController;

@end

// View controller representing a tab switcher. The tab switcher has an
// incognito tab grid, regular tab grid, and remote tabs.
@interface TabGridViewController
    : UIViewController <GridTransitionAnimationLayoutProviding,
                        LayoutSwitcherProvider,
                        TabGridPaging,
                        ViewRevealingAnimatee>

@property(nonatomic, weak) id<ApplicationCommands> handler;
@property(nonatomic, weak) id<IncognitoReauthCommands> reauthHandler;
// Handlers for popup menu commands for the regular and incognito states.
@property(nonatomic, weak) id<PopupMenuCommands> regularPopupMenuHandler;
@property(nonatomic, weak) id<PopupMenuCommands> incognitoPopupMenuHandler;

// Delegate for this view controller to handle presenting tab UI.
@property(nonatomic, weak) id<TabPresentationDelegate> tabPresentationDelegate;

@property(nonatomic, weak) id<TabGridViewControllerDelegate> delegate;

// Consumers send updates from the model layer to the UI layer.
@property(nonatomic, readonly) id<GridConsumer> regularTabsConsumer;
@property(nonatomic, readonly) id<GridConsumer, IncognitoReauthConsumer>
    incognitoTabsConsumer;
@property(nonatomic, readonly) id<RecentTabsConsumer> remoteTabsConsumer;

// Delegates send updates from the UI layer to the model layer.
@property(nonatomic, weak) id<GridCommands> regularTabsDelegate;
@property(nonatomic, weak) id<GridCommands> incognitoTabsDelegate;

// Handles drag and drop interactions that require the model layer.
@property(nonatomic, weak) id<GridDragDropHandler> regularTabsDragDropHandler;
@property(nonatomic, weak) id<GridDragDropHandler> incognitoTabsDragDropHandler;

// Data sources provide lazy access to heavy-weight resources.
@property(nonatomic, weak) id<GridImageDataSource> regularTabsImageDataSource;
@property(nonatomic, weak) id<GridImageDataSource> incognitoTabsImageDataSource;

// Readwrite override of the UIViewController property. This object will ignore
// the value supplied by UIViewController.
@property(nonatomic, weak, readwrite)
    UIViewController* childViewControllerForStatusBarStyle;

// The view controller for remote tabs.
// TODO(crbug.com/845192) : This was only exposed in the public interface so
// that TabGridViewController does not need to know about model objects. The
// model objects used in this view controller should be factored out.
@property(nonatomic, strong)
    RecentTabsTableViewController* remoteTabsViewController;

// Tells the receiver to prepare for its appearance by pre-requesting any
// resources it needs from data sources. This should be called before any
// transitions are triggered.
- (void)prepareForAppearance;

// Notifies the ViewController that its content is being displayed or hidden.
- (void)contentWillAppearAnimated:(BOOL)animated;
- (void)contentDidAppear;
- (void)contentWillDisappearAnimated:(BOOL)animated;

// Notifies the ViewController that the Close All Tabs confirmation action sheet
// has been closed.
- (void)closeAllTabsConfirmationClosed;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_
