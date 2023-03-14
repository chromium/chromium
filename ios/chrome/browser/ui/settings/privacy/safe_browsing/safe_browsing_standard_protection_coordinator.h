// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_STANDARD_PROTECTION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_STANDARD_PROTECTION_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class SafeBrowsingStandardProtectionCoordinator;

// Delegate for Safe Browsing Standard Protection Coordinator.
@protocol SafeBrowsingStandardProtectionCoordinatorDelegate

- (void)safeBrowsingStandardProtectionCoordinatorDidRemove:
    (SafeBrowsingStandardProtectionCoordinator*)coordinator;

@end

// Coordinator for the Safe Browsing Standard Protection view.
@interface SafeBrowsingStandardProtectionCoordinator : ChromeCoordinator

// Delegate.
@property(nonatomic, weak) id<SafeBrowsingStandardProtectionCoordinatorDelegate>
    delegate;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Designated initializer.
// `viewController`: navigation controller.
// `browser`: browser.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_STANDARD_PROTECTION_COORDINATOR_H_
