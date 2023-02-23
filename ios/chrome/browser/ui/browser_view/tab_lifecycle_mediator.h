// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_TAB_LIFECYCLE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_TAB_LIFECYCLE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/browser_view/common_tab_helper_delegate.h"

@protocol CommonTabHelperDelegate;
@class CommandDispatcher;
@class DownloadManagerCoordinator;
@class NewTabPageCoordinator;
@protocol FollowIPHPresenter;
class PrerenderService;
@class PrintController;
@protocol RepostFormTabHelperDelegate;
@class SadTabCoordinator;
@class SideSwipeController;
@protocol SnapshotGeneratorDelegate;
class TabInsertionBrowserAgent;
class WebStateList;
@protocol NetExportTabHelperDelegate;
@protocol NewTabPageTabHelperDelegate;
@protocol PriceNotificationsIPHPresenter;
@protocol SnapshotGeneratorDelegate;

// Mediator that handles the setup of tab helpers that require UI-layer
// dependencies not available when AttachTabHelpers() is called.
// The required dependencies are injected into the mediator instance on init,
// and are generally expected not to change during the mediator's lifetime.
// The mediator keeps only weak references to injected dependencies.
@interface TabLifecycleMediator : NSObject

@property(nonatomic, weak) SideSwipeController* sideSwipeController;
@property(nonatomic, weak)
    DownloadManagerCoordinator* downloadManagerCoordinator;
@property(nonatomic, assign) PrerenderService* prerenderService;
@property(nonatomic, weak) UIViewController* baseViewController;
@property(nonatomic, weak) CommandDispatcher* commandDispatcher;
@property(nonatomic, weak) NewTabPageCoordinator* NTPCoordinator;
@property(nonatomic, weak) id<NetExportTabHelperDelegate> tabHelperDelegate;
@property(nonatomic, weak) id<NewTabPageTabHelperDelegate> NTPTabHelperDelegate;
@property(nonatomic, weak) id<PriceNotificationsIPHPresenter>
    priceNotificationsIPHPresenter;
@property(nonatomic, weak) PrintController* printController;
@property(nonatomic, weak) id<RepostFormTabHelperDelegate> repostFormDelegate;
@property(nonatomic, weak) id<FollowIPHPresenter> followIPHPresenter;
@property(nonatomic, assign) TabInsertionBrowserAgent* tabInsertionBrowserAgent;
@property(nonatomic, weak) id<CommonTabHelperDelegate> delegate;
@property(nonatomic, weak) id<SnapshotGeneratorDelegate>
    snapshotGeneratorDelegate;

// Creates an instance of the mediator. Delegates will be installed into all
// existing web states in `webStateList`. While the mediator is alive,
// delegates will be added and removed from web states when they are inserted
// into or removed from the web state list.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects all delegates set by the mediator on any web states in its
// web state list. After `disconnect` is called, the mediator will not add
// delegates to further webstates.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_TAB_LIFECYCLE_MEDIATOR_H_
