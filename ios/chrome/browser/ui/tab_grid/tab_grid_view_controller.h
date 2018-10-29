// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_grid/transitions/grid_transition_state_providing.h"

@protocol ApplicationCommands;
@protocol GridConsumer;
@protocol GridCommands;
@protocol GridImageDataSource;
@protocol RecentTabsConsumer;
@class RecentTabsTableViewController;

// Delegate protocol for an object that can handle presenting ("opening") tabs
// from the tab grid.
@protocol TabPresentationDelegate<NSObject>
// Show the active tab in |page|, presented on top of the tab grid.  The
// omnibox will be focused after the animation if |focusOmnibox| is YES.
- (void)showActiveTabInPage:(TabGridPage)page focusOmnibox:(BOOL)focusOmnibox;
@end

// View controller representing a tab switcher. The tab switcher has an
// incognito tab grid, regular tab grid, and remote tabs.
@interface TabGridViewController
    : UIViewController<TabGridPaging, GridTransitionStateProviding>

@property(nonatomic, weak) id<ApplicationCommands> dispatcher;

// Delegate for this view controller to handle presenting tab UI.
@property(nonatomic, weak) id<TabPresentationDelegate> tabPresentationDelegate;

// Consumers send updates from the model layer to the UI layer.
@property(nonatomic, readonly) id<GridConsumer> regularTabsConsumer;
@property(nonatomic, readonly) id<GridConsumer> incognitoTabsConsumer;
@property(nonatomic, readonly) id<RecentTabsConsumer> remoteTabsConsumer;

// Delegates send updates from the UI layer to the model layer.
@property(nonatomic, weak) id<GridCommands> regularTabsDelegate;
@property(nonatomic, weak) id<GridCommands> incognitoTabsDelegate;

// Data sources provide lazy access to heavy-weight resources.
@property(nonatomic, weak) id<GridImageDataSource> regularTabsImageDataSource;
@property(nonatomic, weak) id<GridImageDataSource> incognitoTabsImageDataSource;

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

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_VIEW_CONTROLLER_H_
