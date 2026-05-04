// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_feature.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

class PageActionMenuFeatureTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    icon_ = [[UIImage alloc] init];
    feature_ = [[PageActionMenuFeature alloc]
        initWithFeatureType:PageActionMenuTranslate
                      title:@"Translate"
                       icon:icon_
                 actionType:PageActionMenuButtonAction];
  }

  void TearDown() override {
    feature_ = nil;
    icon_ = nil;
    PlatformTest::TearDown();
  }

  PageActionMenuFeature* feature_;
  UIImage* icon_;
};

// Tests that initialization sets the correct properties.
TEST_F(PageActionMenuFeatureTest, Initialization) {
  EXPECT_NE(feature_, nil);
  EXPECT_EQ(feature_.featureType, PageActionMenuTranslate);
  EXPECT_NSEQ(feature_.title, @"Translate");
  EXPECT_EQ(feature_.icon, icon_);
  EXPECT_EQ(feature_.actionType, PageActionMenuButtonAction);
  EXPECT_FALSE(feature_.toggleState);
}

// Tests that setters work correctly.
TEST_F(PageActionMenuFeatureTest, Setters) {
  [feature_ setSubtitle:@"Subtitle"];
  EXPECT_NSEQ(feature_.subtitle, @"Subtitle");

  [feature_ setActionText:@"Action"];
  EXPECT_NSEQ(feature_.actionText, @"Action");

  [feature_ setAccessibilityLabel:@"Accessibility"];
  EXPECT_NSEQ(feature_.accessibilityLabel, @"Accessibility");

  feature_.toggleState = YES;
  EXPECT_TRUE(feature_.toggleState);
}

}  // namespace
