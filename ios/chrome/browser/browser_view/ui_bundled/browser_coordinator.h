// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_COORDINATOR_H_

#include "base/ios/block_types.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class BrowserLayoutViewController;
@class BrowserViewController;

// Coordinator for BrowserViewController.
@interface BrowserCoordinator : ChromeCoordinator

// The main view controller.
@property(nonatomic, strong, readonly) BrowserViewController* viewController;

// The layout view controller managed by this coordinator.
@property(nonatomic, strong, readonly)
    BrowserLayoutViewController* browserLayoutViewController;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_UI_BUNDLED_BROWSER_COORDINATOR_H_
