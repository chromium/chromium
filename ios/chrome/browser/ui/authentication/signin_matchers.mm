// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_matchers.h"

#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
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

id<GREYMatcher> SigninScreenPromoMatcher() {
  return grey_accessibilityID(
      first_run::kFirstRunSignInScreenAccessibilityIdentifier);
}

id<GREYMatcher> SigninScreenPromoPrimaryButtonMatcher() {
  return grey_allOf(
      grey_accessibilityID(kPromoStylePrimaryActionAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> SigninScreenPromoSecondaryButtonMatcher() {
  return grey_allOf(
      grey_accessibilityID(kPromoStyleSecondaryActionAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
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
