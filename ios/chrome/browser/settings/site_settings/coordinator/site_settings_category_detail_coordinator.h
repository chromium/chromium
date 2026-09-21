// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_COORDINATOR_SITE_SETTINGS_CATEGORY_DETAIL_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_COORDINATOR_SITE_SETTINGS_CATEGORY_DETAIL_COORDINATOR_H_

#import "ios/chrome/browser/settings/site_settings/public/site_settings_constants.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol SiteSettingsCategoryDetailViewControllerDelegate;

// Coordinator managing the Category Detail screen in Site Settings.
@interface SiteSettingsCategoryDetailCoordinator : ChromeCoordinator

// Delegate for the category detail view controller.
@property(nonatomic, weak) id<SiteSettingsCategoryDetailViewControllerDelegate>
    delegate;

// Designated initializer.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        category:(SiteSettingsCategory)category
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_COORDINATOR_SITE_SETTINGS_CATEGORY_DETAIL_COORDINATOR_H_
