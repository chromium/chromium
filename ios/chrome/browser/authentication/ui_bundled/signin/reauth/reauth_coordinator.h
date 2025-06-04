// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_REAUTH_REAUTH_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_REAUTH_REAUTH_COORDINATOR_H_

#import "components/signin/public/identity_manager/account_info.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace signin_metrics {
enum class AccessPoint;
}

// The result of the reauth flow.
enum class ReauthResult : int {
  // The reauth flow has completed successfully.
  kSuccess = 0,
  // There was an error during the reauth flow.
  kError = 1,
  // The reauth flow was cancelled by the user.
  kCancelledByUser = 2,
  // The reauth flow was cancelled because the coordinator was stopped.
  kInterrupted = 3,
  kMaxValue = kInterrupted
};

// The delegate for the reauth flow.
@protocol ReauthCoordinatorDelegate

// The reauth flow has completed with `result`.
- (void)reauthFinishedWithResult:(ReauthResult)result;

@end

// Implements a reauthentication flow that asks the user to resolve a persistent
// auth error by entering their credentials again.
@interface ReauthCoordinator : ChromeCoordinator

// The delegate to get notified after the flow has completed.
@property(nonatomic, weak) id<ReauthCoordinatorDelegate> delegate;

// Designated initializer for ReauthCoordinator started from a sign-in flow.
// `identity` - the identity for which the reauthentication flow should be
//         shown.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                   account:(const CoreAccountInfo&)account
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_REAUTH_REAUTH_COORDINATOR_H_
