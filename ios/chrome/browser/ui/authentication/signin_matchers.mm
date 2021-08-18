// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_matchers.h"

#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

id<GREYMatcher> IdentityCellMatcherForEmail(NSString* email) {
  return grey_allOf(grey_accessibilityID(email),
                    grey_kindOfClassName(@"TableViewIdentityCell"),
                    grey_sufficientlyVisible(), nil);
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

id<GREYMatcher> GoogleSyncSettingsButton() {
  return grey_allOf(
      grey_kindOfClass([UITableViewCell class]), grey_sufficientlyVisible(),
      grey_accessibilityID(kSettingsGoogleSyncAndServicesCellId), nil);
}

}  // namespace chrome_test_util
