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

// Dismisses the UI protected by Local Authentication. When there are several
// levels of UI protected by LA , this function results in the dismissal of all
// LA-protected UI levels. For example if the user opens Settings > Password
// Manager > Password Details and LA is canceled, then Password Details and
// Password Manager are dismissed, leaving the user on the Settings page.
- (void)dismissUIAfterFailedReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator;

// Invoked when the coordinator is about to push its view controller.
// Parent coordinators can prepare for reauthentication here, by stopping any
// presented alerts or unregistering observers not needed while reauth is
// ongoing.
- (void)willPushReauthenticationViewController;

@end

// Coordinator for protecting sensitive content with Local
// Authentication (Face Id, Touch Id or Passcode).
//
// When started and until it is stopped, this coordinator detects when the scene
// is backgrounded and requires Local Authentication the next time it is
// foregrounded. When authentication is required, it pushes a view controller in
// its navigation controller so the previous topViewController is hidden until
// successful authentication.
//
// Additionally, this coordinator can also be
// configured to require authentication when it is started. This is intended for
// surfaces that require authentication before revealing their contents. Make
// sure to start it right after pushing the blocked view controller in the
// navigation controller.
//
// In most cases, this coordinator will be started when its parent is started
// and it will have more or less the same lifecycle. Stop it when
// the parent coordinator is stopped or when it presents another child
// coordinator that pushes a view controller in the navigation controller, in
// order to avoid covering content not owned by the parent.
@interface ReauthenticationCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<ReauthenticationCoordinatorDelegate> delegate;

// Creates a coordinator for blocking the top view controller in
// `navigationController`.
//
// - reauthenticationModule: Used for triggering Local Authentication.
// - authOnStart: Whether authentication should be required when this
// coordinator is started.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                          reauthenticationModule:(id<ReauthenticationProtocol>)
                                                     reauthenticationModule
                                     authOnStart:(BOOL)authOnStart
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Stops the coordinator and pops its view controller if it is in the navigation
// stack. Leaves the navigation stack intact if the view controller is not in
// it.
- (void)stopAndPopViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_REAUTHENTICATION_REAUTHENTICATION_COORDINATOR_H_
