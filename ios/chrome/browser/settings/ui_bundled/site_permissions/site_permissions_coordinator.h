// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol SitePermissionsCoordinatorDelegate;

// Coordinator for Site Permissions settings.
@interface SitePermissionsCoordinator : ChromeCoordinator

// Delegate for coordinator events.
@property(nonatomic, weak) id<SitePermissionsCoordinatorDelegate> delegate;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_COORDINATOR_H_
