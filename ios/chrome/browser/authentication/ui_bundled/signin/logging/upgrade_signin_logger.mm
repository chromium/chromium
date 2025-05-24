// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/logging/upgrade_signin_logger.h"

#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "net/base/network_change_notifier.h"

using signin_metrics::AccessPoint;
using signin_metrics::LogSigninAccessPointStarted;
using signin_metrics::PromoAction;
using signin_metrics::RecordSigninUserActionForAccessPoint;

@implementation UpgradeSigninLogger {
  // Identity manager to retrieve Chrome identities.
  raw_ptr<signin::IdentityManager> _identityManager;

  // Account manager service to retrieve Chrome identities.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;

  // View where the sign-in button was displayed.
  signin_metrics::AccessPoint _accessPoint;

  // Promo button used to trigger the sign-in.
  signin_metrics::PromoAction _promoAction;
}

#pragma mark - Public

- (instancetype)initWithAccessPoint:(AccessPoint)accessPoint
                        promoAction:(PromoAction)promoAction
                    identityManager:(signin::IdentityManager*)identityManager
              accountManagerService:
                  (ChromeAccountManagerService*)accountManagerService {
  self = [super init];
  if (self) {
    CHECK(identityManager);
    CHECK(accountManagerService);
    _accessPoint = accessPoint;
    _promoAction = promoAction;
    _identityManager = identityManager;
    _accountManagerService = accountManagerService;
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_accountManagerService);
  DCHECK(!_identityManager);
}

- (void)disconnect {
  _accountManagerService = nullptr;
  _identityManager = nullptr;
}

#pragma mark - SigninLogger

- (void)logSigninStarted {
  if (!_identityManager || !_accountManagerService) {
    return;
  }
  RecordSigninUserActionForAccessPoint(_accessPoint);

  // Records in user defaults that the promo has been shown as well as the
  // number of times it's been displayed.
  signin::RecordUpgradePromoSigninStarted(
      _identityManager, _accountManagerService, version_info::GetVersion());
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  int promoSeenCount =
      [standardDefaults integerForKey:kDisplayedSSORecallPromoCountKey];
  promoSeenCount++;
  [standardDefaults setInteger:promoSeenCount
                        forKey:kDisplayedSSORecallPromoCountKey];

  NSArray<id<SystemIdentity>>* identitiesOnDevice =
      signin::GetIdentitiesOnDevice(_identityManager, _accountManagerService);
  UMA_HISTOGRAM_COUNTS_100(kUMASSORecallAccountsAvailable,
                           [identitiesOnDevice count]);
  UMA_HISTOGRAM_COUNTS_100(kUMASSORecallPromoSeenCount, promoSeenCount);
}

- (void)logSigninCompletedWithResult:(SigninCoordinatorResult)signinResult
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
    case SigninCoordinatorUINotAvailable:
    case SigninCoordinatorResultInterrupted:
    case SigninCoordinatorProfileSwitch: {
      // TODO(crbug.com/40622384): Add metric for when the sign-in has been
      // interrupted.
      break;
    }
  }
}

@end
