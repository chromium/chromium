// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class Browser;
@class TabGroupIndicatorView;

// Coordinator for the tab group indicator.
@interface TabGroupIndicatorCoordinator : ChromeCoordinator

// A read-only reference to the TabGroupIndicatorView instance, which is
// assigned by the PrimaryToolbarCoordinator to its associated viewController.
@property(nonatomic, strong, readonly) TabGroupIndicatorView* view;

// Initializes this Coordinator with its `browser` and a nil base view
// controller.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_COORDINATOR_TAB_GROUP_INDICATOR_COORDINATOR_H_
