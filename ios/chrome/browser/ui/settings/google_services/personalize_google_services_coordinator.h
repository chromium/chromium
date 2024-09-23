// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PERSONALIZE_GOOGLE_SERVICES_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PERSONALIZE_GOOGLE_SERVICES_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class PersonalizeGoogleServicesCoordinator;

// Delegate for PersonalizeGoogleServicesCoordinator.
@protocol PersonalizeGoogleServicesCoordinatorDelegate <NSObject>

// Called when the view controller is removed from its parent.
- (void)personalizeGoogleServicesCoordinatorWasRemoved:
    (PersonalizeGoogleServicesCoordinator*)coordinator;

@end

// Coordinator for the "Personalize Google Services" page.
@interface PersonalizeGoogleServicesCoordinator : ChromeCoordinator

// Delegate
@property(nonatomic, weak) id<PersonalizeGoogleServicesCoordinatorDelegate>
    delegate;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Designated initializer.
// `navigationController`: navigation controller.
// `browser`: browser.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_PERSONALIZE_GOOGLE_SERVICES_COORDINATOR_H_
