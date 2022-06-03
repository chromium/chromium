// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_COORDINATOR_H_

#include "base/ios/block_types.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@class BrowserViewController;

// Coordinator for BrowserViewController.
@interface BrowserCoordinator : ChromeCoordinator

// The main view controller.
@property(nonatomic, strong, readonly) BrowserViewController* viewController;

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
