// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/user_signin_logger.h"

#import "base/metrics/user_metrics.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::RecordAction;
using base::UserMetricsAction;
using signin_metrics::AccessPoint;
using signin_metrics::LogSigninAccessPointStarted;
using signin_metrics::LogSigninAccessPointCompleted;
using signin_metrics::PromoAction;

@implementation UserSigninLogger

#pragma mark - Public

- (instancetype)initWithAccessPoint:(AccessPoint)accessPoint
                        promoAction:(PromoAction)promoAction
                        prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _accessPoint = accessPoint;
    _promoAction = promoAction;
    _prefService = prefService;
  }
  return self;
}

- (void)dealloc {
  DCHECK(!self.prefService);
}

- (void)disconnect {
  self.prefService = nullptr;
}

- (void)logSigninStarted {
  LogSigninAccessPointStarted(self.accessPoint, self.promoAction);
}

- (void)logSigninCompletedWithResult:(SigninCoordinatorResult)signinResult
                        addedAccount:(BOOL)addedAccount
               advancedSettingsShown:(BOOL)advancedSettingsShown {
  switch (signinResult) {
    case SigninCoordinatorResultSuccess: {
      LogSigninAccessPointCompleted(self.accessPoint, self.promoAction);
      if (advancedSettingsShown) {
        RecordAction(
            UserMetricsAction("Signin_Signin_WithAdvancedSyncSettings"));
      } else {
        RecordAction(
            UserMetricsAction("Signin_Signin_WithDefaultSyncSettings"));
      }
      break;
    }
    case SigninCoordinatorResultCanceledByUser: {
      RecordAction(UserMetricsAction("Signin_Undo_Signin"));
      break;
    }
    case SigninCoordinatorResultInterrupted: {
      RecordAction(UserMetricsAction("Signin_Interrupt_Signin"));
      break;
    }
  }
}

@end
