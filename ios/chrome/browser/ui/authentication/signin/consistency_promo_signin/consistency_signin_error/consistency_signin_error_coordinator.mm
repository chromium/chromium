// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_signin_error/consistency_signin_error_coordinator.h"

#import "google_apis/gaia/google_service_auth_error.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_signin_error/consistency_signin_error_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ConsistencySigninErrorCoordinator () <
    ConsistencySigninErrorViewControllerDelegate>

// Error state.
@property(nonatomic, assign) GoogleServiceAuthError::State errorState;
// View controller to display sign-in errors.
@property(nonatomic, strong)
    ConsistencySigninErrorViewController* errorViewController;
@end

@implementation ConsistencySigninErrorCoordinator
@synthesize errorViewController = _errorViewController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                errorState:
                                    (GoogleServiceAuthError::State)errorState {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _errorState = errorState;
  }
  return self;
}

- (void)start {
  DCHECK(self.errorState);
  self.errorViewController = [[ConsistencySigninErrorViewController alloc]
      initWithAuthErrorState:self.errorState];
  self.errorViewController.delegate = self;
  [self.errorViewController view];
}

#pragma mark - ConsistencySigninErrorViewControllerDelegate

- (void)consistencySigninErrorViewControllerDidTapRetrySignin:
    (ConsistencySigninErrorViewController*)viewController {
  DCHECK_EQ(viewController, self.errorViewController);
  [self.delegate consistencySigninErrorCoordinatorRetrySignin];
}

#pragma mark - Properties

- (UIViewController*)viewController {
  return self.errorViewController;
}

@end
