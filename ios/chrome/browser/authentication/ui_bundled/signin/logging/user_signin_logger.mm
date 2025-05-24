// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/logging/user_signin_logger.h"

#import "base/metrics/user_metrics.h"

using base::RecordAction;
using base::UserMetricsAction;
using signin_metrics::AccessPoint;
using signin_metrics::LogSigninAccessPointCompleted;
using signin_metrics::LogSigninAccessPointStarted;
using signin_metrics::LogSignInStarted;
using signin_metrics::PromoAction;

@implementation UserSigninLogger

#pragma mark - Public

- (instancetype)initWithAccessPoint:(AccessPoint)accessPoint
                        promoAction:(PromoAction)promoAction {
  self = [super init];
  if (self) {
    _accessPoint = accessPoint;
    _promoAction = promoAction;
  }
  return self;
}

#pragma mark - SigninLogger

- (void)logSigninStarted {
  LogSignInStarted(self.accessPoint);
  LogSigninAccessPointStarted(self.accessPoint, self.promoAction);
}

- (void)logSigninCompletedWithResult:(SigninCoordinatorResult)signinResult
                        addedAccount:(BOOL)addedAccount {
  switch (signinResult) {
    case SigninCoordinatorResultSuccess: {
      LogSigninAccessPointCompleted(self.accessPoint, self.promoAction);
      RecordAction(UserMetricsAction("Signin_Signin_WithDefaultSyncSettings"));
      break;
    }
    case SigninCoordinatorResultCanceledByUser: {
      RecordAction(UserMetricsAction("Signin_Undo_Signin"));
      break;
    }
    case SigninCoordinatorResultDisabled:
    case SigninCoordinatorResultInterrupted: {
      RecordAction(UserMetricsAction("Signin_Interrupt_Signin"));
      break;
    }
    case SigninCoordinatorProfileSwitch: {
      RecordAction(UserMetricsAction("Signin_Switch_Profile"));
      break;
    }
    case SigninCoordinatorUINotAvailable: {
      RecordAction(UserMetricsAction("Signin_Abort_Signin_UINotAvailable"));
      break;
    }
  }
}

@end
