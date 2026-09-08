// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_COORDINATOR_SITE_SETTINGS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_COORDINATOR_SITE_SETTINGS_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol SiteSettingsCoordinatorDelegate;

// Coordinator for the root Site Settings screen.
@interface SiteSettingsCoordinator : ChromeCoordinator

// Delegate for coordinator events.
@property(nonatomic, weak) id<SiteSettingsCoordinatorDelegate> delegate;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_COORDINATOR_SITE_SETTINGS_COORDINATOR_H_
