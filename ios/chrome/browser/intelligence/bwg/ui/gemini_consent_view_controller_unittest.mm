// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_view_controller.h"

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

// Test fixture for GeminiConsentViewController.
class GeminiConsentViewControllerTest : public PlatformTest {
 public:
  GeminiConsentViewController* CreateViewController(
      BOOL is_account_managed,
      NSString* country = nil,
      BOOL use_strict_consent = NO) {
    GeminiConsentConfiguration* config = [GeminiConsentConfiguration
        configurationForManaged:is_account_managed
                         strict:use_strict_consent
                           type:GeminiFREType::kNewUser
                        country:country];
    GeminiConsentViewController* controller =
        [[GeminiConsentViewController alloc] initWithConfiguration:config];
    // Force view initialization since this view controller is never added into
    // the hierarchy in this unit test.
    [controller view];
    [controller viewWillLayoutSubviews];
    return controller;
  }
};

// Tests initialization with a managed account.
TEST_F(GeminiConsentViewControllerTest, InitializationWithManagedAccount) {
  GeminiConsentViewController* view_controller =
      CreateViewController(YES, @"us");

  EXPECT_NE(nil, view_controller);
  EXPECT_TRUE(view_controller.view);
  EXPECT_TRUE(view_controller.navigationItem.hidesBackButton);
}

// Tests initialization with a non-managed account.
TEST_F(GeminiConsentViewControllerTest, InitializationWithNonManagedAccount) {
  GeminiConsentViewController* view_controller =
      CreateViewController(NO, @"us");

  EXPECT_NE(nil, view_controller);
  EXPECT_TRUE(view_controller.view);
  EXPECT_TRUE(view_controller.navigationItem.hidesBackButton);
}
