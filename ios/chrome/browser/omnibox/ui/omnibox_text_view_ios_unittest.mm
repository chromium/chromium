// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/omnibox_text_view_ios.h"

#import <UIKit/UIKit.h>

#import "base/test/allow_check_is_test_for_testing.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_keyboard_delegate.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_input_delegate.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface OmniboxTextViewIOS (Testing)
- (void)forwardKeyCommandShiftReturn:(UIKeyCommand*)command;
@end

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

// Tests that key commands are registered for Composebox.
TEST_F(OmniboxTextViewIOSTest, KeyCommandsForComposebox) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kComposeboxPhysicalKeyboardReturnKeys);

  NSArray<UIKeyCommand*>* commands = text_view_.keyCommands;

  BOOL hasReturn = NO;
  BOOL hasShiftReturn = NO;
  BOOL hasCommandReturn = NO;

  for (UIKeyCommand* command in commands) {
    if ([command.input isEqualToString:@"\r"]) {
      if (command.modifierFlags == 0) {
        hasReturn = YES;
      } else if (command.modifierFlags == UIKeyModifierCommand) {
        hasCommandReturn = YES;
      } else if (command.modifierFlags == UIKeyModifierShift) {
        hasShiftReturn = YES;
      }
    }
  }

  EXPECT_FALSE(hasReturn);
  EXPECT_FALSE(hasCommandReturn);
  EXPECT_TRUE(hasShiftReturn);
}

// Tests that key commands for LocationBar do not include return keys.
TEST_F(OmniboxTextViewIOSTest, KeyCommandsForLocationBar) {
  text_view_ = [[OmniboxTextViewIOS alloc]
            initWithFrame:CGRectMake(0, 0, 100, 100)
                textColor:[UIColor colorWithWhite:0 alpha:1]
                tintColor:[UIColor colorWithRed:0 green:0 blue:1 alpha:1]
      presentationContext:OmniboxPresentationContext::kLocationBar];

  NSArray<UIKeyCommand*>* commands = text_view_.keyCommands;

  for (UIKeyCommand* command in commands) {
    EXPECT_NSNE(command.input, @"\r");
  }
}

// Tests canPerformAction for Shift+Return is always enabled.
TEST_F(OmniboxTextViewIOSTest, CanPerformActionShiftReturn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kComposeboxPhysicalKeyboardReturnKeys);

  EXPECT_TRUE([text_view_
      canPerformAction:@selector(forwardKeyCommandShiftReturn:)
            withSender:nil]);
}

// Tests that key commands are not registered for Composebox when the feature is
// disabled.
TEST_F(OmniboxTextViewIOSTest, KeyCommandsDisabledByFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kComposeboxPhysicalKeyboardReturnKeys);

  NSArray<UIKeyCommand*>* commands = text_view_.keyCommands;

  for (UIKeyCommand* command in commands) {
    EXPECT_NSNE(command.input, @"\r");
  }
}

// Tests that canPerformAction returns NO for Return keys when the feature is
// disabled.
TEST_F(OmniboxTextViewIOSTest, CanPerformActionReturnDisabledByFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kComposeboxPhysicalKeyboardReturnKeys);

  EXPECT_FALSE([text_view_
      canPerformAction:@selector(forwardKeyCommandShiftReturn:)
            withSender:nil]);
}
// Tests that Shift+Return inserts a newline.
TEST_F(OmniboxTextViewIOSTest, ForwardShiftReturnKey) {
  base::UserActionTester user_action_tester;
  text_view_.text = @"Line 1";

  [text_view_ performSelector:@selector(forwardKeyCommandShiftReturn:)
                   withObject:nil];

  EXPECT_NSEQ(@"Line 1\n", text_view_.text);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "IOS.Omnibox.PhysicalKeyboardShiftReturn"));
}

// Tests that a newline character is blocked by default, triggering
// textInputShouldReturn:
TEST_F(OmniboxTextViewIOSTest, TextInputShouldReturnOnNewline) {
  base::UserActionTester user_action_tester;
  id delegateMock = OCMProtocolMock(@protocol(OmniboxTextInputDelegate));
  text_view_.omniboxTextInputDelegate = delegateMock;

  OCMExpect([delegateMock textInputShouldReturn:text_view_]);

  [(id<UITextViewDelegate>)text_view_ textView:text_view_
                       shouldChangeTextInRange:NSMakeRange(0, 0)
                               replacementText:@"\n"];

  [delegateMock verify];
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "IOS.Omnibox.PhysicalKeyboardShiftReturn"));
}

// Tests that carriage return characters are also blocked by default, triggering
// textInputShouldReturn:
TEST_F(OmniboxTextViewIOSTest, TextInputShouldReturnOnCarriageReturn) {
  base::UserActionTester user_action_tester;
  id delegateMock = OCMProtocolMock(@protocol(OmniboxTextInputDelegate));
  text_view_.omniboxTextInputDelegate = delegateMock;

  OCMExpect([delegateMock textInputShouldReturn:text_view_]);

  [(id<UITextViewDelegate>)text_view_ textView:text_view_
                       shouldChangeTextInRange:NSMakeRange(0, 0)
                               replacementText:@"\r"];

  [delegateMock verify];
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "IOS.Omnibox.PhysicalKeyboardShiftReturn"));
}

// Tests that paragraph separator characters are not blocked, and do not trigger
// textInputShouldReturn:
TEST_F(OmniboxTextViewIOSTest, TextInputShouldNotReturnOnParagraphSeparator) {
  base::UserActionTester user_action_tester;
  id delegateMock = OCMProtocolMock(@protocol(OmniboxTextInputDelegate));
  text_view_.omniboxTextInputDelegate = delegateMock;

  OCMReject([delegateMock textInputShouldReturn:text_view_]);
  OCMExpect([delegateMock textInput:text_view_
                shouldChangeTextInRange:NSMakeRange(0, 0)
                      replacementString:@"\u2029"])
      .andReturn(YES);

  BOOL result = [(id<UITextViewDelegate>)text_view_ textView:text_view_
                                     shouldChangeTextInRange:NSMakeRange(0, 0)
                                             replacementText:@"\u2029"];
  EXPECT_TRUE(result);

  [delegateMock verify];
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "IOS.Omnibox.PhysicalKeyboardShiftReturn"));
}

}  // namespace
