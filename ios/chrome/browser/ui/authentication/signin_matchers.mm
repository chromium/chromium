// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_matchers.h"

#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace chrome_test_util {

id<GREYMatcher> IdentityCellMatcherForEmail(NSString* email) {
  return grey_allOf(grey_accessibilityID(email),
                    grey_kindOfClassName(@"TableViewIdentityCell"),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> AdvancedSyncSettingsDoneButtonMatcher() {
  return grey_accessibilityID(kAdvancedSyncSettingsDoneButtonMatcherId);
}

id<GREYMatcher> SettingsLink() {
  return grey_allOf(grey_accessibilityLabel(@"settings"),
                    grey_accessibilityTrait(UIAccessibilityTraitLink),
                    grey_interactable(), nil);
}

id<GREYMatcher> WebSigninSkipButtonMatcher() {
  return grey_allOf(
      grey_accessibilityID(kWebSigninSkipButtonAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> WebSigninPrimaryButtonMatcher() {
  return grey_allOf(
      grey_accessibilityID(kWebSigninPrimaryButtonAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> GoogleSyncSettingsButton() {
  return grey_allOf(grey_kindOfClass([UITableViewCell class]),
                    grey_accessibilityID(kSettingsGoogleSyncAndServicesCellId),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> UpgradeSigninPromoMatcher() {
  return grey_accessibilityID(kUnifiedConsentScrollViewIdentifier);
}

id<GREYMatcher> SettingsSignInRowMatcher() {
  return grey_allOf(grey_accessibilityID(kSettingsSignInCellId),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> HistoryOptInPrimaryButtonMatcher() {
  return grey_accessibilityID(kPromoStylePrimaryActionAccessibilityIdentifier);
}

}  // namespace chrome_test_util
