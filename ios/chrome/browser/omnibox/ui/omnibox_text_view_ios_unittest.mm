// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/omnibox_text_view_ios.h"

#import <UIKit/UIKit.h>

#import "base/test/allow_check_is_test_for_testing.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_keyboard_delegate.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_input_delegate.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface OmniboxTextViewIOS (Testing)
- (void)forwardKeyCommandShiftReturn:(UIKeyCommand*)command;
@end

@interface FakeOmniboxTextViewHeightDelegate
    : NSObject <OmniboxTextViewHeightDelegate>
@property(nonatomic, assign) NSUInteger contentChangedCallCount;
@property(nonatomic, weak) OmniboxTextViewIOS* lastTextView;
@end

@implementation FakeOmniboxTextViewHeightDelegate

- (void)textViewContentChanged:(OmniboxTextViewIOS*)textView {
  self.contentChangedCallCount++;
  self.lastTextView = textView;
}

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

  void TearDown() override {
    text_view_.heightDelegate = nil;
    [container_view_ removeFromSuperview];
    window_.hidden = YES;
    text_view_ = nil;
    container_view_ = nil;
    window_ = nil;
    PlatformTest::TearDown();
  }

  // Attaches `container_view_` to `window_` if needed, applies the given trait
  // overrides, flushes traits and layout, and checks resolved traits.
  void ApplyTraits(UIUserInterfaceSizeClass horizontal_size_class,
                   UIUserInterfaceSizeClass vertical_size_class,
                   UIContentSizeCategory content_size_category =
                       UIContentSizeCategoryLarge) {
    if (!window_) {
      window_ = [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 400, 400)];
      [window_ addSubview:container_view_];
    }
    container_view_.traitOverrides.preferredContentSizeCategory =
        content_size_category;
    container_view_.traitOverrides.horizontalSizeClass = horizontal_size_class;
    container_view_.traitOverrides.verticalSizeClass = vertical_size_class;
    [container_view_ updateTraitsIfNeeded];
    [container_view_ setNeedsLayout];
    [container_view_ layoutIfNeeded];
    [text_view_ updateTraitsIfNeeded];
    EXPECT_EQ(text_view_.traitCollection.horizontalSizeClass,
              horizontal_size_class);
    EXPECT_EQ(text_view_.traitCollection.verticalSizeClass,
              vertical_size_class);
    EXPECT_NSEQ(text_view_.traitCollection.preferredContentSizeCategory,
                content_size_category);
  }

  UIWindow* window_ = nil;
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

// Tests that setting placeholderColor updates the placeholder label textColor.
TEST_F(OmniboxTextViewIOSTest, PlaceholderColor) {
  UIColor* color = [UIColor colorWithWhite:0.5 alpha:1.0];
  text_view_.placeholderColor = color;
  EXPECT_NSEQ(color, text_view_.placeholderColor);
  EXPECT_NSEQ(color, text_view_.placeholderLabel.textColor);
}

// Tests that key commands are registered for Composebox.
TEST_F(OmniboxTextViewIOSTest, KeyCommandsForComposebox) {
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
  EXPECT_TRUE([text_view_
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

// Tests that changing horizontal size class updates the font, placeholder
// font, cached single-line height, and container insets while preserving text
// selection.
TEST_F(OmniboxTextViewIOSTest,
       UpdatesFontLineHeightAndInsetsOnHorizontalSizeClassChange) {
  ApplyTraits(UIUserInterfaceSizeClassRegular, UIUserInterfaceSizeClassRegular,
              UIContentSizeCategoryLarge);
  text_view_.text = @"Hello world";
  text_view_.selectedRange = NSMakeRange(2, 5);
  CGFloat regular_point_size = text_view_.font.pointSize;
  CGFloat regular_line_height = [text_view_ singleLineHeight];
  CGFloat regular_top_inset = text_view_.textContainerInset.top;
  FakeOmniboxTextViewHeightDelegate* height_delegate =
      [[FakeOmniboxTextViewHeightDelegate alloc] init];
  text_view_.heightDelegate = height_delegate;

  ApplyTraits(UIUserInterfaceSizeClassCompact, UIUserInterfaceSizeClassRegular,
              UIContentSizeCategoryLarge);

  EXPECT_LT(text_view_.font.pointSize, regular_point_size);
  EXPECT_NSEQ(text_view_.placeholderLabel.font, text_view_.font);
  EXPECT_LT([text_view_ singleLineHeight], regular_line_height);
  EXPECT_LT(text_view_.textContainerInset.top, regular_top_inset);
  EXPECT_EQ(height_delegate.contentChangedCallCount, 1u);
  EXPECT_EQ(height_delegate.lastTextView, text_view_);
  EXPECT_EQ(text_view_.selectedRange.location, 2u);
  EXPECT_EQ(text_view_.selectedRange.length, 5u);
}

// Tests that changing the preferred content size category in Compact width
// updates fonts and typing attributes in place, preserves inline autocomplete
// highlights, and notifies the height delegate even when insets do not change.
TEST_F(OmniboxTextViewIOSTest,
       UpdatesFontsAndPreservesAutocompleteHighlightOnContentSizeChange) {
  ApplyTraits(UIUserInterfaceSizeClassCompact, UIUserInterfaceSizeClassRegular,
              UIContentSizeCategoryLarge);
  NSAttributedString* text =
      [[NSAttributedString alloc] initWithString:@"chr_ome"];
  [text_view_ setText:text userTextLength:3];
  UIColor* highlight_before =
      [text_view_.attributedText attribute:NSBackgroundColorAttributeName
                                   atIndex:4
                            effectiveRange:nullptr];
  ASSERT_NE(highlight_before, nil);
  CGFloat initial_point_size = text_view_.font.pointSize;
  CGFloat initial_line_height = [text_view_ singleLineHeight];
  UIEdgeInsets initial_insets = text_view_.textContainerInset;
  FakeOmniboxTextViewHeightDelegate* height_delegate =
      [[FakeOmniboxTextViewHeightDelegate alloc] init];
  text_view_.heightDelegate = height_delegate;

  ApplyTraits(UIUserInterfaceSizeClassCompact, UIUserInterfaceSizeClassRegular,
              UIContentSizeCategoryExtraExtraLarge);

  EXPECT_GT(text_view_.font.pointSize, initial_point_size);
  EXPECT_NSEQ(text_view_.placeholderLabel.font, text_view_.font);
  EXPECT_NSEQ(text_view_.typingAttributes[NSFontAttributeName],
              text_view_.font);
  NSRange full_range = NSMakeRange(0, text_view_.attributedText.length);
  NSRange effective_range = NSMakeRange(0, 0);
  EXPECT_NSEQ([text_view_.attributedText attribute:NSFontAttributeName
                                           atIndex:0
                             longestEffectiveRange:&effective_range
                                           inRange:full_range],
              text_view_.font);
  EXPECT_EQ(effective_range.location, 0u);
  EXPECT_EQ(effective_range.length, full_range.length);
  EXPECT_NSEQ(
      [text_view_.attributedText attribute:NSBackgroundColorAttributeName
                                   atIndex:4
                            effectiveRange:nullptr],
      highlight_before);
  EXPECT_GT([text_view_ singleLineHeight], initial_line_height);
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(text_view_.textContainerInset,
                                            initial_insets));
  EXPECT_EQ(height_delegate.contentChangedCallCount, 1u);
  EXPECT_EQ(height_delegate.lastTextView, text_view_);
}

// Tests that changing vertical size class in Regular width updates container
// insets and notifies the height delegate.
TEST_F(OmniboxTextViewIOSTest, UpdatesInsetsOnVerticalSizeClassChange) {
  ApplyTraits(UIUserInterfaceSizeClassRegular, UIUserInterfaceSizeClassRegular);
  CGFloat regular_top_inset = text_view_.textContainerInset.top;
  FakeOmniboxTextViewHeightDelegate* height_delegate =
      [[FakeOmniboxTextViewHeightDelegate alloc] init];
  text_view_.heightDelegate = height_delegate;

  ApplyTraits(UIUserInterfaceSizeClassRegular, UIUserInterfaceSizeClassCompact);

  EXPECT_LT(text_view_.textContainerInset.top, regular_top_inset);
  EXPECT_EQ(height_delegate.contentChangedCallCount, 1u);
  EXPECT_EQ(height_delegate.lastTextView, text_view_);
}

// Tests that rotating a Compact-width device between Regular and Compact
// vertical size classes is a no-op even when leading emoji triggers fallback
// font substitution.
TEST_F(OmniboxTextViewIOSTest,
       PreservesSelectionAndSkipsUpdateOnCompactVerticalSizeClassChange) {
  ApplyTraits(UIUserInterfaceSizeClassCompact, UIUserInterfaceSizeClassRegular);
  text_view_.text = @"🍎 Hello world";
  text_view_.selectedRange = NSMakeRange(5, 0);
  FakeOmniboxTextViewHeightDelegate* height_delegate =
      [[FakeOmniboxTextViewHeightDelegate alloc] init];
  text_view_.heightDelegate = height_delegate;

  ApplyTraits(UIUserInterfaceSizeClassCompact, UIUserInterfaceSizeClassCompact);

  EXPECT_EQ(height_delegate.contentChangedCallCount, 0u);
  EXPECT_EQ(text_view_.selectedRange.location, 5u);
  EXPECT_EQ(text_view_.selectedRange.length, 0u);
}

// Tests that cached single-line height and placeholder font still update on a
// deferred trait flush even if `-setText:` was called before traits flushed.
TEST_F(OmniboxTextViewIOSTest,
       UpdatesCachedLineHeightWhenTextSetBeforeDeferredTraitFlush) {
  ApplyTraits(UIUserInterfaceSizeClassRegular, UIUserInterfaceSizeClassRegular);
  CGFloat regular_line_height = [text_view_ singleLineHeight];
  container_view_.traitOverrides.horizontalSizeClass =
      UIUserInterfaceSizeClassCompact;
  text_view_.text = @"Hello world";

  [container_view_ updateTraitsIfNeeded];
  [container_view_ setNeedsLayout];
  [container_view_ layoutIfNeeded];
  [text_view_ updateTraitsIfNeeded];

  EXPECT_LT([text_view_ singleLineHeight], regular_line_height);
  EXPECT_NSEQ(text_view_.placeholderLabel.font, text_view_.font);
}

}  // namespace
