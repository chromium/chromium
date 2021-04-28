// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_SIGNIN_ERROR_CONSISTENCY_SIGNIN_ERROR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_SIGNIN_ERROR_CONSISTENCY_SIGNIN_ERROR_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "google_apis/gaia/google_service_auth_error.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@class ConsistencySigninErrorCoordinator;

@protocol ConsistencySigninErrorCoordinatorDelegate <NSObject>

// Retries the sign-in in case of an authentication error.
- (void)consistencySigninErrorCoordinatorRetrySignin;

@end

// Coordinator that presents an error message to the user based on the
// authentication service error state.
@interface ConsistencySigninErrorCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Designated initializer.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                errorState:
                                    (GoogleServiceAuthError::State)errorState
    NS_DESIGNATED_INITIALIZER;

@property(nonatomic, strong, readonly) UIViewController* viewController;
@property(nonatomic, weak) id<ConsistencySigninErrorCoordinatorDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_SIGNIN_ERROR_CONSISTENCY_SIGNIN_ERROR_COORDINATOR_H_
