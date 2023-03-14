// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol ApplicationCommands;
@class GoogleServicesSettingsCoordinator;

// Delegate for GoogleServicesSettingsCoordinator.
@protocol GoogleServicesSettingsCoordinatorDelegate

// Called when the view controller is removed from navigation controller.
- (void)googleServicesSettingsCoordinatorDidRemove:
    (GoogleServicesSettingsCoordinator*)coordinator;

@end

// Coordinator for the Google services settings view.
// All the sync changes made by the user are applied when
// -[GoogleServicesSettingsCoordinator stop] is called, or when the
// GoogleServicesSettingsCoordinator instance is deallocated.
@interface GoogleServicesSettingsCoordinator : ChromeCoordinator

// View controller for the Google services settings.
@property(nonatomic, strong) UIViewController* viewController;
// Delegate.
@property(nonatomic, weak) id<GoogleServicesSettingsCoordinatorDelegate>
    delegate;
// Whether the Google services settings view is at the top of the navigation
// stack. This does not necessarily mean the view is displayed to the user since
// it can be obstructed by views that are not owned by the navigation stack
// (e.g. MyGoogle UI).
@property(nonatomic, assign, readonly) BOOL googleServicesSettingsViewIsShown;

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

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_COORDINATOR_H_
