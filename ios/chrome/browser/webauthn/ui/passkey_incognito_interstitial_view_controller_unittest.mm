// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/ui/passkey_incognito_interstitial_view_controller.h"

#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

class PasskeyIncognitoInterstitialViewControllerTest : public PlatformTest {
 protected:
  PasskeyIncognitoInterstitialViewControllerTest() {}
  ~PasskeyIncognitoInterstitialViewControllerTest() override {}
};

TEST_F(PasskeyIncognitoInterstitialViewControllerTest, PropertiesAreSetOnLoad) {
  PasskeyIncognitoInterstitialViewController* view_controller =
      [[PasskeyIncognitoInterstitialViewController alloc] init];

  [view_controller loadViewIfNeeded];

  EXPECT_NSEQ(view_controller.titleString,
              l10n_util::GetNSString(
                  IDS_IOS_PASSKEY_INCOGNITO_INTERSTITIAL_DIALOG_TITLE));
  EXPECT_NSEQ(view_controller.subtitleString,
              l10n_util::GetNSString(
                  IDS_IOS_PASSKEY_INCOGNITO_INTERSTITIAL_DIALOG_SUBTITLE));
  EXPECT_NSEQ(view_controller.configuration.primaryActionString,
              l10n_util::GetNSString(
                  IDS_IOS_PASSKEY_INCOGNITO_INTERSTITIAL_CONTINUE_BUTTON));
  EXPECT_NSEQ(view_controller.configuration.secondaryActionString,
              l10n_util::GetNSString(
                  IDS_IOS_PASSKEY_INCOGNITO_INTERSTITIAL_CANCEL_BUTTON));
}
