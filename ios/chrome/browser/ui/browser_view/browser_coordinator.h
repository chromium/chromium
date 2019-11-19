// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_COORDINATOR_H_

#include "base/ios/block_types.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@protocol ApplicationCommands;
@class BrowserViewController;
@class TabModel;

class AppUrlLoadingService;

// Coordinator for BrowserViewController.
@interface BrowserCoordinator : ChromeCoordinator

// Use only -initWithBaseViewController:browser:
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
    NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;

// The main view controller.
@property(nonatomic, strong, readonly) BrowserViewController* viewController;

// Command handler for ApplicationCommands.
@property(nonatomic, weak) id<ApplicationCommands> applicationCommandHandler;

// The application level component for url loading. Should be used only by
// browser state level UrlLoadingService instances.
@property(nonatomic, assign) AppUrlLoadingService* appURLLoadingService;

// The tab model.
@property(nonatomic, weak, readonly) TabModel* tabModel;

// Activates/deactivates the object. This will enable/disable the ability for
// this object to browse, and to have live UIWebViews associated with it. While
// not active, the UI will not react to changes in the tab model, so generally
// an inactive BVC should not be visible.
@property(nonatomic, assign, getter=isActive) BOOL active;

// Clears any presented state on BVC.
- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_COORDINATOR_H_
