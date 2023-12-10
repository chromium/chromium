// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

NSString* const kSkipSigninAccessibilityIdentifier =
    @"SkipSigninAccessibilityIdentifier";
NSString* const kAddAccountAccessibilityIdentifier =
    @"AddAccountAccessibilityIdentifier";
NSString* const kConfirmationAccessibilityIdentifier =
    @"ConfirmationAccessibilityIdentifier";
NSString* const kHistorySyncViewAccessibilityIdentifier =
    @"HistorySyncViewAccessibilityIdentifier";
NSString* const kMoreAccessibilityIdentifier = @"MoreAccessibilityIdentifier";
NSString* const kWebSigninAccessibilityIdentifier =
    @"WebSigninAccessibilityIdentifier";
NSString* const kWebSigninPrimaryButtonAccessibilityIdentifier =
    @"WebSigninPrimaryButtonAccessibilityIdentifier";
NSString* const kWebSigninSkipButtonAccessibilityIdentifier =
    @"WebSigninSkipButtonAccessibilityIdentifier";
NSString* const kTangibleSyncViewAccessibilityIdentifier =
    @"TangibleSyncViewAccessibilityIdentifier";
NSString* const kConsistencyAccountChooserAddAccountIdentifier =
    @"ConsistencyAccountChooserAddAccountIdentifier";

const char* kWebSigninConsistencyConsecutiveActiveDismissalLimitParam =
    "consecutive_active_dismissal_limit";

const int kDefaultWebSignInDismissalCount = 3;

NSString* const kDisplayedSSORecallForMajorVersionKey =
    @"DisplayedSSORecallForMajorVersionKey";
NSString* const kLastShownAccountGaiaIdVersionKey =
    @"LastShownAccountGaiaIdVersionKey";
NSString* const kSigninPromoViewDisplayCountKey =
    @"SigninPromoViewDisplayCountKey";
NSString* const kDisplayedSSORecallPromoCountKey =
    @"DisplayedSSORecallPromoCount";
const char* const kUMASSORecallPromoAction = "SSORecallPromo.PromoAction";
const char* const kUMASSORecallAccountsAvailable =
    "SSORecallPromo.AccountsAvailable";
const char* const kUMASSORecallPromoSeenCount = "SSORecallPromo.PromoSeenCount";
