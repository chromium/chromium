// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/user_signin_logger.h"

#import "base/metrics/user_metrics.h"

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
              accountManagerService:
                  (ChromeAccountManagerService*)accountManagerService {
  self = [super init];
  if (self) {
    DCHECK(accountManagerService);
    _accountManagerService = accountManagerService;
    _accessPoint = accessPoint;
    _promoAction = promoAction;
  }
  return self;
}

- (void)dealloc {
  // TODO(crbug.com/1464966): Switch back to DCHECK if the number of reports is
  // low.
  DUMP_WILL_BE_CHECK(!self.accountManagerService);
}

- (void)disconnect {
  self.accountManagerService = nullptr;
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
    case SigninCoordinatorResultDisabled:
    case SigninCoordinatorResultInterrupted: {
      RecordAction(UserMetricsAction("Signin_Interrupt_Signin"));
      break;
    }
  }
}

@end
