// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/promo/contextual/coordinator/contextual_default_browser_promo_mediator.h"

#import "ios/chrome/browser/default_browser/promo/contextual/ui/contextual_default_browser_promo_consumer.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

class ContextualDefaultBrowserPromoMediatorTest : public PlatformTest {
 protected:
  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    PlatformTest::TearDown();
  }

  ContextualDefaultBrowserPromoMediator* mediator_ = nil;
};

// Tests that setting the consumer for the Gemini promo configures all strings,
// assets, and color providers.
TEST_F(ContextualDefaultBrowserPromoMediatorTest,
       TestConsumerPopulationForGemini) {
  id mock_consumer =
      OCMStrictProtocolMock(@protocol(ContextualDefaultBrowserPromoConsumer));

  OCMExpect([mock_consumer
      setPromoTitle:l10n_util::GetNSString(
                        IDS_IOS_DEFAULT_BROWSER_CONTEXTUAL_GEMINI_TITLE)]);
  OCMExpect([mock_consumer
      setPromoSubtitle:
          l10n_util::GetNSString(
              IDS_IOS_DEFAULT_BROWSER_CONTEXTUAL_GEMINI_SUBTITLE)]);
  OCMExpect([mock_consumer setAnimationAssetName:@"FRE_Summarize_Slide"]);
  OCMExpect([mock_consumer
      setLightModeColorProvider:SummarizeSlideLightModeColorProvider()
          darkModeColorProvider:SummarizeSlideDarkModeColorProvider()]);

  mediator_ = [[ContextualDefaultBrowserPromoMediator alloc]
      initWithPromoType:ContextualDefaultBrowserPromoType::kGemini];
  mediator_.consumer = mock_consumer;

  EXPECT_OCMOCK_VERIFY(mock_consumer);
}
