// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_TABS_TABS_SETTINGS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_TABS_TABS_SETTINGS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class TabsSettingsCoordinator;

// Delegate for TabsSettingsCoordinator.
@protocol TabsSettingsCoordinatorDelegate
// Called when the UI for the coordinator is removed / dismissed.
- (void)tabsSettingsCoordinatorDidRemove:(TabsSettingsCoordinator*)coordinator;
@end

// Coordinator for the tabs settings.
@interface TabsSettingsCoordinator : ChromeCoordinator

// Delegate.
@property(nonatomic, weak) id<TabsSettingsCoordinatorDelegate> delegate;

// Designated initializer.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_TABS_TABS_SETTINGS_COORDINATOR_H_
