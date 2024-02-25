// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/carousel/omnibox_popup_carousel_control.h"

#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/ui/omnibox/popup/carousel/carousel_item.h"
#import "ios/chrome/browser/ui/omnibox/popup/carousel/carousel_item_menu_provider.h"
#import "ios/chrome/browser/ui/omnibox/popup/carousel/omnibox_popup_carousel_control_unittest_util.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class OmniboxPopupCarouselControlTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    delegate_ = [OCMockObject
        mockForProtocol:@protocol(OmniboxPopupCarouselControlDelegate)];
    menu_provider_ =
        [OCMockObject mockForProtocol:@protocol(CarouselItemMenuProvider)];

    carousel_item_ = [[CarouselItem alloc] init];
    carousel_item_.title = @"Some title";

    carousel_control_ = [[OmniboxPopupCarouselControl alloc] init];
    carousel_control_.delegate = delegate_;
    // Configure control with `carousel_item_`. This adds the subviews to the
    // control.
    carousel_control_.carouselItem = carousel_item_;
  }

  OCMockObject<OmniboxPopupCarouselControlDelegate>* delegate_;
  OCMockObject<CarouselItemMenuProvider>* menu_provider_;
  OmniboxPopupCarouselControl* carousel_control_;
  CarouselItem* carousel_item_;
};

// Tests that the control width is equal to the
// `kOmniboxPopupCarouselControlWidth`.
TEST_F(OmniboxPopupCarouselControlTest, WidthIsEqualToConstant) {
  // Compute the size of the control with unlimited space.
  CGSize sizeThatFits = [carousel_control_ sizeThatFits:CGSizeZero];
  EXPECT_EQ(sizeThatFits.width, kOmniboxPopupCarouselControlWidth);
}

// Tests that the accessibility label matches the displayed text.
TEST_F(OmniboxPopupCarouselControlTest, AccessibilityText) {
  EXPECT_NSEQ(carousel_item_.title, carousel_control_.accessibilityLabel);
}

// Tests that the label of CarouselItem is present and correct.
TEST_F(OmniboxPopupCarouselControlTest, LabelIsPresentAndCorrect) {
  UILabel* label = GetLabelFromCarouselControl(carousel_control_);
  EXPECT_TRUE(label);
  EXPECT_EQ(label.text, carousel_item_.title);
}

// Tests that the delegate is called when selecting the control.
TEST_F(OmniboxPopupCarouselControlTest, DelegateCarouselControlSelection) {
  [[delegate_ expect] carouselControlDidBecomeFocused:carousel_control_];
  carousel_control_.selected = YES;
  [delegate_ verify];
}

}  // namespace
