// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/default_browser_string_util.h"

#include "ios/chrome/grit/ios_google_chrome_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
