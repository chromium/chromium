// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_message_test_utils.h"

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace signin {

id<GREYMatcher> snackbarMessageMatcher(FakeSystemIdentity* identity) {
  NSString* snackbarMessage =
      l10n_util::GetNSStringF(IDS_IOS_ACCOUNT_MENU_SWITCH_CONFIRMATION_TITLE,
                              base::SysNSStringToUTF16(identity.userGivenName));
  return grey_allOf(grey_text(snackbarMessage), grey_sufficientlyVisible(),
                    nil);
}

void assertSnackbarNotShown(FakeSystemIdentity* identity) {
  [[EarlGrey selectElementWithMatcher:snackbarMessageMatcher(identity)]
      assertWithMatcher:grey_nil()];
}

void assertSnackbarShownAndDismissItWithIdentity(FakeSystemIdentity* identity) {
  id<GREYMatcher> snackbar_matcher = snackbarMessageMatcher(identity);
  ConditionBlock wait_for_appearance = ^{
    NSError* error;

    // Checking if collection view exists in the UI hierarchy.
    [[EarlGrey selectElementWithMatcher:snackbar_matcher]
        assertWithMatcher:grey_notNil()
                    error:&error];

    return error == nil;
  };
  if (!wait_for_appearance()) {
    // Waiting up to 10 seconds because sign-out from managed account may be
    // slow.
    GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                   base::Seconds(10), wait_for_appearance),
               @"Snackbar did not appear.");
  }

  [[EarlGrey selectElementWithMatcher:snackbar_matcher]
      performAction:grey_tap()];
}

}  // namespace signin
