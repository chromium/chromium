// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/payments/ui/credit_card_suggestion_bottom_sheet_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/autofill/payments/ui/credit_card_suggestion_bottom_sheet_handler.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

constexpr char kRtlIdnUrl[] =
    "https://paypal.com.xn--4gbrim.xn--ngbc5azd/checkout";
constexpr char16_t kExpectedLtrWrappedPunycodeHost[] =
    u"\x202a"
    u"paypal.com.xn--4gbrim.xn--ngbc5azd"
    u"\x202c";
constexpr char kUrlWithTrivialSubdomain[] = "https://www.example.com/checkout";
constexpr char16_t kExpectedLtrWrappedHostWithoutTrivialSubdomain[] =
    u"\x202a"
    u"example.com"
    u"\x202c";

class CreditCardSuggestionBottomSheetViewControllerTest : public PlatformTest {
 protected:
  CreditCardSuggestionBottomSheetViewControllerTest() {
    handler_ =
        OCMProtocolMock(@protocol(CreditCardSuggestionBottomSheetHandler));
  }

  id<CreditCardSuggestionBottomSheetHandler> handler_;
  CreditCardSuggestionBottomSheetViewController* view_controller_;
};

// Test that an IDN URL containing strong RTL characters is formatted as
// punycode and wrapped in LTR directional formatting in the subtitle.
TEST_F(CreditCardSuggestionBottomSheetViewControllerTest,
       SubtitleFormatsRtlIdnAsPunycodeWithLtrWrapping) {
  view_controller_ = [[CreditCardSuggestionBottomSheetViewController alloc]
      initWithHandler:handler_
                  URL:GURL(kRtlIdnUrl)];
  [view_controller_ loadViewIfNeeded];

  NSString* expected_subtitle = l10n_util::GetNSStringF(
      IDS_IOS_PAYMENT_BOTTOM_SHEET_SUBTITLE, kExpectedLtrWrappedPunycodeHost);
  EXPECT_NSEQ(expected_subtitle, view_controller_.subtitleString);
}

// Test that a trivial "www." subdomain is omitted from the subtitle.
TEST_F(CreditCardSuggestionBottomSheetViewControllerTest,
       SubtitleOmitsTrivialSubdomain) {
  view_controller_ = [[CreditCardSuggestionBottomSheetViewController alloc]
      initWithHandler:handler_
                  URL:GURL(kUrlWithTrivialSubdomain)];
  [view_controller_ loadViewIfNeeded];

  NSString* expected_subtitle =
      l10n_util::GetNSStringF(IDS_IOS_PAYMENT_BOTTOM_SHEET_SUBTITLE,
                              kExpectedLtrWrappedHostWithoutTrivialSubdomain);
  EXPECT_NSEQ(expected_subtitle, view_controller_.subtitleString);
}

}  // namespace
