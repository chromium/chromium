// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/logging/upgrade_signin_logger.h"

#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "net/base/network_change_notifier.h"

using signin_metrics::AccessPoint;
using signin_metrics::LogSigninAccessPointStarted;
using signin_metrics::PromoAction;
using signin_metrics::RecordSigninUserActionForAccessPoint;

@implementation UpgradeSigninLogger

#pragma mark - Public

+ (void)logSigninStartedWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
                  accountManagerService:
                      (ChromeAccountManagerService*)accountManagerService {
  if (!accountManagerService) {
    return;
  }
  RecordSigninUserActionForAccessPoint(accessPoint);

  // Records in user defaults that the promo has been shown as well as the
  // number of times it's been displayed.
  signin::RecordUpgradePromoSigninStarted(accountManagerService,
                                          version_info::GetVersion());
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  int promoSeenCount =
      [standardDefaults integerForKey:kDisplayedSSORecallPromoCountKey];
  promoSeenCount++;
  [standardDefaults setInteger:promoSeenCount
                        forKey:kDisplayedSSORecallPromoCountKey];

  NSArray* identities = accountManagerService->GetAllIdentities();
  UMA_HISTOGRAM_COUNTS_100(kUMASSORecallAccountsAvailable, [identities count]);
  UMA_HISTOGRAM_COUNTS_100(kUMASSORecallPromoSeenCount, promoSeenCount);
}

+ (void)logSigninCompletedWithResult:(SigninCoordinatorResult)signinResult
                        addedAccount:(BOOL)addedAccount {
  switch (signinResult) {
    case SigninCoordinatorResultSuccess: {
      UserSigninPromoAction promoAction = addedAccount
                                              ? PromoActionAddedAnotherAccount
                                              : PromoActionEnabledSSOAccount;
      UMA_HISTOGRAM_ENUMERATION(kUMASSORecallPromoAction, promoAction,
                                PromoActionCount);
      break;
    }
    case SigninCoordinatorResultCanceledByUser: {
      UMA_HISTOGRAM_ENUMERATION(kUMASSORecallPromoAction, PromoActionDismissed,
                                PromoActionCount);
      break;
    }
    case SigninCoordinatorResultDisabled:
    case SigninCoordinatorResultInterrupted: {
      // TODO(crbug.com/40622384): Add metric for when the sign-in has been
      // interrupted.
      break;
    }
  }
}

- (void)logSigninStarted {
  [super logSigninStarted];
  [UpgradeSigninLogger
      logSigninStartedWithAccessPoint:self.accessPoint
                accountManagerService:self.accountManagerService];
}

- (void)logSigninCompletedWithResult:(SigninCoordinatorResult)signinResult
                        addedAccount:(BOOL)addedAccount {
  [super logSigninCompletedWithResult:signinResult addedAccount:addedAccount];
  [UpgradeSigninLogger logSigninCompletedWithResult:signinResult
                                       addedAccount:addedAccount];
}

@end
