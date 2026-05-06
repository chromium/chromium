// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_accordion_view.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class GeminiConsentAccordionViewTest : public PlatformTest {
 protected:
  void SetUp() override { PlatformTest::SetUp(); }
};

// Tests that the accordion view creates chevrons and hides body in collapsible
// mode.
TEST_F(GeminiConsentAccordionViewTest, CollapsibleMode) {
  GeminiConsentRow* item = [[GeminiConsentRow alloc]
      initWithIcon:[[UIImage alloc] init]
             title:@"Title"
              body:[[NSAttributedString alloc] initWithString:@"Body"]];
  GeminiConsentAccordionView* view =
      [[GeminiConsentAccordionView alloc] initWithRows:@[ item ]
                                           collapsible:YES];

  EXPECT_NE(nil, view);

  // GeminiConsentAccordionView -> UIStackView -> GeminiConsentRowView ->
  // UIStackView (row)
  NSArray* subviews = view.subviews;
  ASSERT_EQ(1U, subviews.count);
  UIStackView* containerStack = static_cast<UIStackView*>(subviews[0]);
  ASSERT_EQ(1U, containerStack.arrangedSubviews.count);
  UIView* rowView = static_cast<UIView*>(containerStack.arrangedSubviews[0]);
  ASSERT_EQ(1U, rowView.subviews.count);
  UIStackView* row = static_cast<UIStackView*>(rowView.subviews[0]);

  // Collapsible row should have 3 subviews: Icon, Content, Chevron
  ASSERT_EQ(3U, row.arrangedSubviews.count);

  // Check that body is hidden by default in collapsible mode.
  UIStackView* contentStack =
      static_cast<UIStackView*>(row.arrangedSubviews[1]);
  ASSERT_EQ(2U, contentStack.arrangedSubviews.count);
  UIView* bodyView = contentStack.arrangedSubviews[1];
  EXPECT_TRUE(bodyView.hidden);
}

// Tests that the accordion view does not create chevrons and shows body in
// non-collapsible mode.
TEST_F(GeminiConsentAccordionViewTest, NonCollapsibleMode) {
  GeminiConsentRow* item = [[GeminiConsentRow alloc]
      initWithIcon:[[UIImage alloc] init]
             title:@"Title"
              body:[[NSAttributedString alloc] initWithString:@"Body"]];
  GeminiConsentAccordionView* view =
      [[GeminiConsentAccordionView alloc] initWithRows:@[ item ]
                                           collapsible:NO];

  EXPECT_NE(nil, view);

  NSArray* subviews = view.subviews;
  ASSERT_EQ(1U, subviews.count);
  UIStackView* containerStack = static_cast<UIStackView*>(subviews[0]);
  ASSERT_EQ(1U, containerStack.arrangedSubviews.count);
  UIView* rowView = static_cast<UIView*>(containerStack.arrangedSubviews[0]);
  ASSERT_EQ(1U, rowView.subviews.count);
  UIStackView* row = static_cast<UIStackView*>(rowView.subviews[0]);

  // Non-collapsible row should have 2 subviews: Icon, Content (No Chevron)
  ASSERT_EQ(2U, row.arrangedSubviews.count);

  // Check that body is visible by default in non-collapsible mode.
  UIStackView* contentStack =
      static_cast<UIStackView*>(row.arrangedSubviews[1]);
  ASSERT_EQ(2U, contentStack.arrangedSubviews.count);
  UIView* bodyView = contentStack.arrangedSubviews[1];
  EXPECT_FALSE(bodyView.hidden);
}
