// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/default_browser_string_util.h"

#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using DefaultBrowserStringUtilTest = PlatformTest;

// Tests strings for promo.
TEST_F(DefaultBrowserStringUtilTest, DefaultStrings) {
  const std::map<std::string, std::string> feature_params = {
      {"show_switch_title", "true"}};

  NSString* actualTitle = GetDefaultBrowserPromoTitle();
  NSString* expectedTitle = l10n_util::GetNSString(
      IDS_IOS_DEFAULT_BROWSER_TITLE_CTA_EXPERIMENT_SWITCH);
  ASSERT_NSEQ(expectedTitle, actualTitle);

  NSString* actualLearnMore = GetDefaultBrowserLearnMoreText();
  NSString* expectedLearnMore = l10n_util::GetNSString(
      IDS_IOS_DEFAULT_BROWSER_LEARN_MORE_INSTRUCTIONS_MESSAGE_CTA_EXPERIMENT);
  ASSERT_NSEQ(expectedLearnMore, actualLearnMore);
}

}  // namespace
