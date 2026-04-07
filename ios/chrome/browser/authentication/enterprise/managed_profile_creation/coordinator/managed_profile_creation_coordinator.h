// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_ENTERPRISE_MANAGED_PROFILE_CREATION_COORDINATOR_MANAGED_PROFILE_CREATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_ENTERPRISE_MANAGED_PROFILE_CREATION_COORDINATOR_MANAGED_PROFILE_CREATION_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class ManagedProfileCreationCoordinator;
@class ManagedProfileCreationMediator;
namespace signin {
enum class ManagedAccountSigninMode;
}  // namespace signin
@protocol SystemIdentity;

@protocol ManagedProfileCreationCoordinatorDelegate <NSObject>

// Called when the user accepted to continue to sign-in with a managed account.
// If the user cancelled, the result is nullopt. Otherwise, the result either
// stated what the user selected or why there was no selection available.
- (void)managedProfileCreationCoordinator:
            (ManagedProfileCreationCoordinator*)coordinator
                                   result:(std::optional<
                                              signin::ManagedAccountSigninMode>)
                                              mode;

// Called when the Managed Profile Creation must be stopped, without the user
// having tapped on one of the view’s button. This should be very rare. It can
// occurs either if the profile was removed from the device (this could be done
// from another application), or if the user signed-in into an account (this
// could be done by using a second view, with a managed profile, and switching
// to a personal profile).
- (void)managedProfileCreationCoordinatorWantsToBeStopped:
    (ManagedProfileCreationCoordinator*)coordinator;

@end

// Coordinator to present managed profile creation.
@interface ManagedProfileCreationCoordinator : ChromeCoordinator

// Creates a coordinator for a ManagedProfileCreationViewController shown
// in `viewController`. UIViewController::presentViewController will be used
// to show the ViewController created and owned by
// ManagedProfileCreationCoordinator.
// `identity` must be non nil.
// `hostedDomain` may be nil.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                  identity:(id<SystemIdentity>)identity
                              hostedDomain:(NSString*)hostedDomain
                                   browser:(Browser*)browser
                                      mode:
                                          (signin::ManagedAccountSigninMode)mode
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@property(nonatomic, weak) id<ManagedProfileCreationCoordinatorDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_ENTERPRISE_MANAGED_PROFILE_CREATION_COORDINATOR_MANAGED_PROFILE_CREATION_COORDINATOR_H_
