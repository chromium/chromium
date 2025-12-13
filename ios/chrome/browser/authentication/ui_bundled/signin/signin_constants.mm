// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"

NSString* const kHistorySyncViewAccessibilityIdentifier =
    @"HistorySyncViewAccessibilityIdentifier";
NSString* const kConsistencySigninAccessibilityIdentifier =
    @"WebSigninAccessibilityIdentifier";
NSString* const kConsistencySigninPrimaryButtonAccessibilityIdentifier =
    @"WebSigninPrimaryButtonAccessibilityIdentifier";
NSString* const kConsistencySigninSkipButtonAccessibilityIdentifier =
    @"WebSigninSkipButtonAccessibilityIdentifier";
NSString* const kConsistencyAccountChooserAddAccountIdentifier =
    @"ConsistencyAccountChooserAddAccountIdentifier";

NSString* const kManagedProfileCreationScreenAccessibilityIdentifier =
    @"ManagedProfileCreationScreenAccessibilityIdentifier";

NSString* const kBrowsingDataManagementScreenAccessibilityIdentifier =
    @"BrowsingDataManagementScreenAccessibilityIdentifier";

NSString* const kManagedProfileCreationNavigationBarAccessibilityIdentifier =
    @"ManagedProfileCreationNavigationBarAccessibilityIdentifier";

NSString* const kBrowsingDataButtonAccessibilityIdentifier =
    @"BrowsingDataButtonAccessibilityIdentifier";

NSString* const kKeepBrowsingDataSeparateCellId =
    @"KeepBrowsingDataSeparateCellId";

NSString* const kMergeBrowsingDataCellId = @"MergeBrowsingDataCellId";

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
NSString* const kFullscreenSigninPromoManagerMigrationDone =
    @"FullscreenSigninPromoManagerMigrationDone";

NSString* const kManagedProfileLearnMoreURL =
    @"internal://managed-profile-creation-learn-more";
