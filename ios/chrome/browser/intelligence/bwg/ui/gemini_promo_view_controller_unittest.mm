// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_promo_view_controller.h"

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_mutator.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Helper function to find a UILabel with a given text recursively.
UILabel* FindLabelWithText(UIView* view, NSString* text) {
  if ([view isKindOfClass:[UILabel class]]) {
    UILabel* label = static_cast<UILabel*>(view);
    if ([label.text isEqualToString:text]) {
      return label;
    }
  }
  for (UIView* subview in view.subviews) {
    UILabel* result = FindLabelWithText(subview, text);
    if (result) {
      return result;
    }
  }
  return nil;
}

}  // namespace

class GeminiPromoViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    view_controller_ = [[GeminiPromoViewController alloc] init];
    mock_mutator_ = OCMProtocolMock(@protocol(GeminiConsentMutator));
    view_controller_.mutator = mock_mutator_;
  }

  void TearDown() override {
    view_controller_ = nil;
    mock_mutator_ = nil;
    PlatformTest::TearDown();
  }

  void LoadView() {
    // Triggers viewDidLoad.
    [view_controller_ view];
    [view_controller_ viewWillAppear:NO];
  }

  GeminiPromoViewController* view_controller_;
  id mock_mutator_;
};

// Tests that the view controller can be initialized successfully.
TEST_F(GeminiPromoViewControllerTest, TestInitialization) {
  EXPECT_NE(nil, view_controller_);
  OCMStub([mock_mutator_ shouldShowImageRemixRow]).andReturn(NO);
  LoadView();
  EXPECT_TRUE(view_controller_.view);
}

// Tests that the image remix row is visible when enabled by the mutator.
TEST_F(GeminiPromoViewControllerTest, ShowsImageRemixRowWhenEnabled) {
  OCMStub([mock_mutator_ shouldShowImageRemixRow]).andReturn(YES);
  LoadView();

  NSString* remixTitle =
      l10n_util::GetNSString(IDS_IOS_GEMINI_PROMO_REMIX_IMAGE_BOX_TITLE);
  UILabel* remixLabel = FindLabelWithText(view_controller_.view, remixTitle);
  EXPECT_NE(nil, remixLabel);
}

// Tests that the image remix row is hidden when disabled by the mutator.
TEST_F(GeminiPromoViewControllerTest, HidesImageRemixRowWhenDisabled) {
  OCMStub([mock_mutator_ shouldShowImageRemixRow]).andReturn(NO);
  LoadView();

  NSString* remixTitle =
      l10n_util::GetNSString(IDS_IOS_GEMINI_PROMO_REMIX_IMAGE_BOX_TITLE);
  UILabel* remixLabel = FindLabelWithText(view_controller_.view, remixTitle);
  EXPECT_EQ(nil, remixLabel);
}
