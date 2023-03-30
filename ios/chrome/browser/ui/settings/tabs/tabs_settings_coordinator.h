// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TABS_SETTINGS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TABS_SETTINGS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator for the tabs settings view.
@interface TabsSettingsCoordinator : ChromeCoordinator

// Designated initializer.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TABS_SETTINGS_COORDINATOR_H_
