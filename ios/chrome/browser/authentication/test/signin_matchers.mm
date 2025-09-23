// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/test/signin_matchers.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/views/views_constants.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace chrome_test_util {

id<GREYMatcher> IdentityCellMatcherForEmail(NSString* email) {
  return grey_allOf(grey_accessibilityID(email),
                    grey_kindOfClassName(@"TableViewIdentityCell"),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> ConsistencySigninSkipButtonMatcher() {
  return grey_allOf(
      grey_accessibilityID(kConsistencySigninSkipButtonAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> ConsistencySigninPrimaryButtonMatcher() {
  return grey_allOf(grey_accessibilityID(
                        kConsistencySigninPrimaryButtonAccessibilityIdentifier),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> AccountChooserButtonMatcher(id<SystemIdentity> identity) {
  if (identity) {
    NSString* accessibility_label = l10n_util::GetNSStringF(
        IDS_IOS_SIGNIN_ACCOUNT_PICKER_DESCRIPTION_WITH_NAME_AND_EMAIL,
        base::SysNSStringToUTF16(identity.userFullName),
        base::SysNSStringToUTF16(identity.userEmail));
    return grey_allOf(grey_accessibilityID(kIdentityButtonControlIdentifier),
                      grey_accessibilityLabel(accessibility_label),
                      grey_sufficientlyVisible(), nil);
  }
  return grey_allOf(grey_accessibilityID(kIdentityButtonControlIdentifier),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> SigninScreenPromoMatcher() {
  return grey_accessibilityID(
      first_run::kFirstRunSignInScreenAccessibilityIdentifier);
}

id<GREYMatcher> SettingsSignInRowMatcher() {
  return grey_allOf(grey_accessibilityID(kSettingsSignInCellId),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> HistoryOptInPromoMatcher() {
  return grey_allOf(
      grey_accessibilityID(kHistorySyncViewAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

id<GREYAction> HistoryOptInScrollDown() {
  return grey_scrollInDirection(kGREYDirectionDown, 200);
}

}  // namespace chrome_test_util
