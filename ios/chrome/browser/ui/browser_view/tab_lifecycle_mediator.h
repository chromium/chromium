// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_TAB_LIFECYCLE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_TAB_LIFECYCLE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/browser_view/common_tab_helper_delegate.h"

@class DownloadManagerCoordinator;
class PrerenderService;
@class SadTabCoordinator;
@class SideSwipeController;
@protocol SnapshotGeneratorDelegate;
class WebStateList;
@protocol NetExportTabHelperDelegate;

typedef struct {
  SideSwipeController* sideSwipeController;
  DownloadManagerCoordinator* downloadManagerCoordinator;
  PrerenderService* prerenderService;
  UIViewController* baseViewController;
  CommandDispatcher* commandDispatcher;
  id<NetExportTabHelperDelegate> tabHelperDelegate;
} TabLifecycleDependencies;

// Mediator that handles the setup of tab helpers that require UI-layer
// dependencies not available when AttachTabHelpers() is called.
// The required dependencies are injected into the mediator instance on init,
// and are generally expected not to change during the mediator's lifetime.
// The mediator keeps only weak references to injected dependencies.
@interface TabLifecycleMediator : NSObject

// Creates an instance of the mediator. Delegates will be installed into all
// existing web states in `webStateList`. While the mediator is alive,
// delegates will be added and removed from web states when they are inserted
// into or removed from the web state list.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                            delegate:(id<CommonTabHelperDelegate>)delegate
           snapshotGeneratorDelegate:
               (id<SnapshotGeneratorDelegate>)snapshotGeneratorDelegate
                        dependencies:(TabLifecycleDependencies)dependencies;

// Disconnects all delegates set by the mediator on any web states in its
// web state list. After `disconnect` is called, the mediator will not add
// delegates to further webstates.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_TAB_LIFECYCLE_MEDIATOR_H_
