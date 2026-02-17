// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/omnibox_text_view_ios.h"

#import <UIKit/UIKit.h>

#import "base/test/allow_check_is_test_for_testing.h"
#import "base/test/task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class OmniboxTextViewIOSTest : public PlatformTest {
 protected:
  void SetUp() override {
    base::test::AllowCheckIsTestForTesting();
    PlatformTest::SetUp();
    // Create a container view to hold both text view and placeholder
    container_view_ = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)];

    text_view_ = [[OmniboxTextViewIOS alloc]
              initWithFrame:CGRectMake(0, 0, 100, 100)
                  textColor:[UIColor blackColor]
                  tintColor:[UIColor blueColor]
        presentationContext:OmniboxPresentationContext::kComposebox];
    [container_view_ addSubview:text_view_];

    // Setup placeholder
    UILabel* placeholderLabel = [[UILabel alloc] init];
    placeholderLabel.text = @"Placeholder Text";
    [container_view_ addSubview:placeholderLabel];

    // Now that both are in the hierarchy, we can set the property which
    // activates constraints
    text_view_.placeholderLabel = placeholderLabel;
  }

  OmniboxTextViewIOS* text_view_;
  UIView* container_view_;
  base::test::TaskEnvironment task_environment_;
};

// Tests that the accessibility value is the placeholder text when the text view
// is empty.
TEST_F(OmniboxTextViewIOSTest, AccessibilityValueWhenEmpty) {
  text_view_.text = @"";
  EXPECT_NSEQ(@"Placeholder Text", text_view_.accessibilityValue);
}

// Tests that the accessibility value is the text content when the text view is
// not empty.
TEST_F(OmniboxTextViewIOSTest, AccessibilityValueWhenNotEmpty) {
  text_view_.text = @"User Text";
  EXPECT_NSEQ(@"User Text", text_view_.accessibilityValue);
}

// Tests that the testing value is correct when the text view is empty.
TEST_F(OmniboxTextViewIOSTest, TextValueForTestingWhenEmpty) {
  text_view_.text = @"";
  EXPECT_NSEQ(@"||||||||", text_view_.textValueForTesting);
}

// Tests that the testing value is correct when the text view is not empty.
TEST_F(OmniboxTextViewIOSTest, TextValueForTestingWhenNotEmpty) {
  text_view_.text = @"User Text";
  EXPECT_NSEQ(@"User Text||||||||", text_view_.textValueForTesting);
}

// Tests that the testing value is correct with autocomplete text.
TEST_F(OmniboxTextViewIOSTest, TextValueForTestingWithAutocomplete) {
  NSAttributedString* text =
      [[NSAttributedString alloc] initWithString:@"User TextAutocomplete"];
  [text_view_ setText:text userTextLength:9];
  EXPECT_NSEQ(@"User Text||||Autocomplete||||", text_view_.textValueForTesting);
}

// Tests that the testing value is correct with additional text.
TEST_F(OmniboxTextViewIOSTest, TextValueForTestingWithAdditionalText) {
  text_view_.text = @"User Text";
  [text_view_ setAdditionalText:@"Additional"];
  EXPECT_NSEQ(@"User Text||||||||Additional", text_view_.textValueForTesting);
}

// Tests that the testing value is correct with autocomplete and additional
// text.
TEST_F(OmniboxTextViewIOSTest, TextValueForTestingWithBoth) {
  NSAttributedString* text =
      [[NSAttributedString alloc] initWithString:@"User TextAutocomplete"];
  [text_view_ setText:text userTextLength:9];
  [text_view_ setAdditionalText:@"Additional"];
  EXPECT_NSEQ(@"User Text||||Autocomplete||||Additional",
              text_view_.textValueForTesting);
}

// Tests that the placeholder label itself is hidden from accessibility.
TEST_F(OmniboxTextViewIOSTest, PlaceholderHiddenFromAccessibility) {
  EXPECT_FALSE(text_view_.placeholderLabel.isAccessibilityElement);
}

}  // namespace
