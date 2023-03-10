// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_ENHANCED_PROTECTION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_ENHANCED_PROTECTION_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class SafeBrowsingEnhancedProtectionCoordinator;

// Delegate for SafeBrowsingEnhancedProtectionCoordinator.
@protocol SafeBrowsingEnhancedProtectionCoordinatorDelegate

// Called when the view controller is removed from navigation controller.
- (void)safeBrowsingEnhancedProtectionCoordinatorDidRemove:
    (SafeBrowsingEnhancedProtectionCoordinator*)coordinator;

@end

// Coordinator for the privacy safe browsing enhanced protection view.
@interface SafeBrowsingEnhancedProtectionCoordinator : ChromeCoordinator

// Delegate.
@property(nonatomic, weak) id<SafeBrowsingEnhancedProtectionCoordinatorDelegate>
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

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_ENHANCED_PROTECTION_COORDINATOR_H_
