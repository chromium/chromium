// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class ConsistencyDefaultAccountCoordinator;
@protocol ConsistencyLayoutDelegate;
@protocol SystemIdentity;

namespace signin_metrics {
enum class AccessPoint : int;
}  // namespace signin_metrics

@protocol ConsistencyDefaultAccountCoordinatorDelegate <NSObject>

// Called when the user wants to skip the consistency promo.
- (void)consistencyDefaultAccountCoordinatorSkip:
    (ConsistencyDefaultAccountCoordinator*)coordinator;

// Called when the user wants to choose a different identity.
- (void)consistencyDefaultAccountCoordinatorOpenIdentityChooser:
    (ConsistencyDefaultAccountCoordinator*)coordinator;

// Called when the user wants to sign-in with the default identity.
- (void)consistencyDefaultAccountCoordinatorSignin:
    (ConsistencyDefaultAccountCoordinator*)coordinator;

// Called when the user wants to sign in without an existing account.
- (void)consistencyDefaultAccountCoordinatorOpenAddAccount:
    (ConsistencyDefaultAccountCoordinator*)coordinator;

@end

// This coordinator presents an entry point to the Chrome sign-in flow with the
// default account available on the device.
@interface ConsistencyDefaultAccountCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@property(nonatomic, strong, readonly) UIViewController* viewController;
@property(nonatomic, weak) id<ConsistencyDefaultAccountCoordinatorDelegate>
    delegate;
@property(nonatomic, weak) id<ConsistencyLayoutDelegate> layoutDelegate;
// This property can be used only after the coordinator is started.
@property(nonatomic, strong) id<SystemIdentity> selectedIdentity;

// Starts the spinner and disables buttons.
- (void)startSigninSpinner;
// Stops the spinner and enables buttons.
- (void)stopSigninSpinner;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_DEFAULT_ACCOUNT_CONSISTENCY_DEFAULT_ACCOUNT_COORDINATOR_H_
