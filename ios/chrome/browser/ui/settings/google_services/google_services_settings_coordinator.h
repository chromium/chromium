// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_mode.h"

@protocol ApplicationCommands;
@protocol BrowserCommands;
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
// Global dispatcher.
@property(nonatomic, weak) id<ApplicationCommands, BrowserCommands> dispatcher;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
    NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Designated initializer.
// |viewController|: navigation controller.
// |browser|: browser.
// |mode|: mode to display the Google services settings.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      mode:(GoogleServicesSettingsMode)mode
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_GOOGLE_SERVICES_SETTINGS_COORDINATOR_H_
