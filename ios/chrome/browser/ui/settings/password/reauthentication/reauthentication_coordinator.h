// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_REAUTHENTICATION_REAUTHENTICATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_REAUTHENTICATION_REAUTHENTICATION_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class ReauthenticationCoordinator;
@class UINavigationController;
@protocol ReauthenticationProtocol;

@protocol ReauthenticationCoordinatorDelegate <NSObject>

// Invoked when Local Authentication is successful and
// ReauthenticationViewController is popped from the navigation
// controller.
- (void)successfulReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator;

@end

// Coordinator that pushes a ReauthenticationViewController in a navigation
// controller. Blocks the content in the navigation controller until until Local
// Authentication (Face Id, Touch Id or Passcode) is passed. Start this
// coordinator from the `start` implementation of the coordinator of the view
// controller that must be blocked, right after pushing the blocked view
// controller in the navigation controller.
@interface ReauthenticationCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<ReauthenticationCoordinatorDelegate> delegate;

// Creates a coordinator for blocking the top view controller in
// `navigationController`.
//
// - reauthenticationModule: Used for triggering Local Authentication.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                          reauthenticationModule:(id<ReauthenticationProtocol>)
                                                     reauthenticationModule
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_REAUTHENTICATION_REAUTHENTICATION_COORDINATOR_H_
