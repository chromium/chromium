// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/zero_state_suggestions/ui/gemini_zero_state_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/ui/gemini_zero_state_mutator.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/zero_state_suggestions_service.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

// Creates a ZeroStateSuggestion with the given text and query.
ZeroStateSuggestion* CreateSuggestion(NSString* text, NSString* query) {
  ZeroStateSuggestion* suggestion = [[ZeroStateSuggestion alloc] init];
  suggestion.text = text;
  suggestion.query = query;
  return suggestion;
}

// Recursively finds all visible chip UIButton instances (with titles) in the
// view hierarchy.
NSArray<UIButton*>* FindButtons(UIView* view) {
  return chrome_test_util::FindViewsWithPredicate<UIButton>(
      view, ^BOOL(UIView* subview) {
        UIButton* button = base::apple::ObjCCast<UIButton>(subview);
        return !button.hidden && button.configuration.title.length > 0;
      });
}

// Recursively finds the close UIButton instance in the view hierarchy.
UIButton* FindCloseButton(UIView* view) {
  return chrome_test_util::FindViewWithPredicate<UIButton>(
      view, ^BOOL(UIView* subview) {
        UIButton* button = base::apple::ObjCCast<UIButton>(subview);
        return button.configuration.image && !button.configuration.title.length;
      });
}

}  // namespace

// Test fixture for GeminiZeroStateViewController.
class GeminiZeroStateViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    view_controller_ = [[GeminiZeroStateViewController alloc] init];
    mock_mutator_ = OCMProtocolMock(@protocol(GeminiZeroStateMutator));
    view_controller_.mutator = mock_mutator_;
  }

  void TearDown() override {
    view_controller_ = nil;
    mock_mutator_ = nil;
    PlatformTest::TearDown();
  }

  GeminiZeroStateViewController* view_controller_ = nil;
  id mock_mutator_ = nil;
};

// Tests that the view controller initializes correctly and sets its mutator.
TEST_F(GeminiZeroStateViewControllerTest, Initialization) {
  EXPECT_NE(view_controller_, nil);
  EXPECT_EQ(view_controller_.mutator, mock_mutator_);
}

// Tests that the view hierarchy loads successfully with 3 empty placeholder
// suggestion buttons (medium, long, short) while waiting for suggestions to
// generate.
TEST_F(GeminiZeroStateViewControllerTest,
       ViewDidLoadCreatesPlaceholderButtons) {
  [view_controller_ loadViewIfNeeded];
  EXPECT_NE(view_controller_.view, nil);

  view_controller_.view.frame = CGRectMake(0, 0, 375, 400);
  [view_controller_.view layoutIfNeeded];

  NSArray<UIButton*>* buttons = FindButtons(view_controller_.view);
  ASSERT_EQ(buttons.count, 3u);
  for (NSUInteger i = 0; i < buttons.count; ++i) {
    SCOPED_TRACE(testing::Message() << "Button index " << i);
    EXPECT_NSEQ(buttons[i].configuration.title, @" ");
    EXPECT_FALSE(buttons[i].userInteractionEnabled);
    EXPECT_FLOAT_EQ(buttons[i].titleLabel.alpha, 0.0);
  }
  EXPECT_NEAR(buttons[0].frame.size.width, 200.0, 1.0);
  EXPECT_NEAR(buttons[1].frame.size.width, 260.0, 1.0);
  EXPECT_NEAR(buttons[2].frame.size.width, 140.0, 1.0);
}

// Tests that setting suggestions reuses the placeholder buttons in-place,
// enables interaction, fades in the title label, and resizes them to fit their
// text.
TEST_F(GeminiZeroStateViewControllerTest,
       SetSuggestionsFillsAndResizesPlaceholderButtons) {
  [view_controller_ loadViewIfNeeded];
  view_controller_.view.frame = CGRectMake(0, 0, 375, 400);
  [view_controller_.view layoutIfNeeded];

  NSArray<UIButton*>* placeholder_buttons = FindButtons(view_controller_.view);
  ASSERT_EQ(placeholder_buttons.count, 3u);
  UIButton* first_placeholder = placeholder_buttons[0];
  CGFloat placeholder_width = first_placeholder.frame.size.width;

  ZeroStateSuggestion* suggestion1 = CreateSuggestion(@"Hi", @"Hi");
  ZeroStateSuggestion* suggestion2 = CreateSuggestion(
      @"Provide a detailed summary of the main points on this page",
      @"Summarize");
  ZeroStateSuggestion* suggestion3 =
      CreateSuggestion(@"Key takeaways", @"Takeaways");
  [view_controller_
      setZeroStateSuggestions:@[ suggestion1, suggestion2, suggestion3 ]];

  NSArray<UIButton*>* filled_buttons = FindButtons(view_controller_.view);
  ASSERT_EQ(filled_buttons.count, 3u);
  EXPECT_EQ(filled_buttons[0], first_placeholder);
  for (NSUInteger i = 0; i < filled_buttons.count; ++i) {
    SCOPED_TRACE(testing::Message() << "Button index " << i);
    EXPECT_TRUE(filled_buttons[i].userInteractionEnabled);
    EXPECT_FLOAT_EQ(filled_buttons[i].titleLabel.alpha, 1.0);
  }
  EXPECT_LT(filled_buttons[0].frame.size.width, placeholder_width);
  EXPECT_GT(filled_buttons[1].frame.size.width, placeholder_width);
}

// Tests that when fewer than 3 suggestions arrive, unused buttons are removed,
// the top buttons slide down to take their place, and the compressed height
// of the view adapts.
TEST_F(GeminiZeroStateViewControllerTest,
       FewerSuggestionsRemovesUnusedButtonsAndAdaptsHeight) {
  [view_controller_ loadViewIfNeeded];
  view_controller_.view.frame = CGRectMake(0, 0, 375, 400);
  [view_controller_.view layoutIfNeeded];

  NSArray<UIButton*>* placeholder_buttons = FindButtons(view_controller_.view);
  ASSERT_EQ(placeholder_buttons.count, 3u);
  UIButton* first_button = placeholder_buttons[0];
  CGRect initial_first_frame_in_view =
      [first_button convertRect:first_button.bounds
                         toView:view_controller_.view];
  CGRect bottom_placeholder_frame_in_view =
      [placeholder_buttons[2] convertRect:placeholder_buttons[2].bounds
                                   toView:view_controller_.view];

  CGSize target_size = CGSizeMake(375, UILayoutFittingCompressedSize.height);
  CGFloat three_chips_compressed_height =
      [view_controller_.view
            systemLayoutSizeFittingSize:target_size
          withHorizontalFittingPriority:UILayoutPriorityRequired
                verticalFittingPriority:UILayoutPriorityFittingSizeLevel]
          .height;

  ZeroStateSuggestion* suggestion1 =
      CreateSuggestion(@"Summarize page", @"Summarize");
  [view_controller_ setZeroStateSuggestions:@[ suggestion1 ]];

  NSArray<UIButton*>* buttons = FindButtons(view_controller_.view);
  ASSERT_EQ(buttons.count, 1u);
  EXPECT_EQ(buttons[0], first_button);

  CGRect updated_first_frame_in_view =
      [first_button convertRect:first_button.bounds
                         toView:view_controller_.view];
  // The remaining top button should slide down to the bottom position where the
  // third button was located.
  EXPECT_GT(updated_first_frame_in_view.origin.y,
            initial_first_frame_in_view.origin.y);
  EXPECT_NEAR(CGRectGetMaxY(updated_first_frame_in_view),
              CGRectGetMaxY(bottom_placeholder_frame_in_view), 1.0);

  CGFloat one_chip_compressed_height =
      [view_controller_.view
            systemLayoutSizeFittingSize:target_size
          withHorizontalFittingPriority:UILayoutPriorityRequired
                verticalFittingPriority:UILayoutPriorityFittingSizeLevel]
          .height;
  EXPECT_LT(one_chip_compressed_height, three_chips_compressed_height);
}

// Tests that setting suggestions to nil resets the view back to 3 empty
// placeholder buttons.
TEST_F(GeminiZeroStateViewControllerTest,
       SetNilSuggestionsResetsToPlaceholderButtons) {
  [view_controller_ loadViewIfNeeded];

  ZeroStateSuggestion* suggestion1 =
      CreateSuggestion(@"Summarize page", @"Summarize");
  [view_controller_ setZeroStateSuggestions:@[ suggestion1 ]];
  ASSERT_EQ(FindButtons(view_controller_.view).count, 1u);

  [view_controller_ setZeroStateSuggestions:nil];

  NSArray<UIButton*>* buttons = FindButtons(view_controller_.view);
  ASSERT_EQ(buttons.count, 3u);
  for (NSUInteger i = 0; i < buttons.count; ++i) {
    SCOPED_TRACE(testing::Message() << "Button index " << i);
    EXPECT_NSEQ(buttons[i].configuration.title, @" ");
    EXPECT_FALSE(buttons[i].userInteractionEnabled);
  }
}

// Tests that setting suggestions before the view is loaded creates the
// expected buttons once the view is loaded.
TEST_F(GeminiZeroStateViewControllerTest, SetSuggestionsBeforeViewLoad) {
  ZeroStateSuggestion* suggestion1 =
      CreateSuggestion(@"Summarize page", @"Summarize");
  ZeroStateSuggestion* suggestion2 =
      CreateSuggestion(@"Key takeaways", @"Takeaways");
  [view_controller_ setZeroStateSuggestions:@[ suggestion1, suggestion2 ]];

  [view_controller_ loadViewIfNeeded];

  NSArray<UIButton*>* buttons = FindButtons(view_controller_.view);
  ASSERT_EQ(buttons.count, 2u);
  EXPECT_NSEQ(buttons[0].configuration.title, @"Summarize page");
  EXPECT_NSEQ(buttons[1].configuration.title, @"Key takeaways");
}

// Tests that updating suggestions replaces existing buttons with new ones.
TEST_F(GeminiZeroStateViewControllerTest, UpdateSuggestions) {
  [view_controller_ loadViewIfNeeded];

  ZeroStateSuggestion* initial_suggestion =
      CreateSuggestion(@"Initial", @"Initial");
  [view_controller_ setZeroStateSuggestions:@[ initial_suggestion ]];

  NSArray<UIButton*>* initial_buttons = FindButtons(view_controller_.view);
  ASSERT_EQ(initial_buttons.count, 1u);
  EXPECT_NSEQ(initial_buttons[0].configuration.title, @"Initial");

  ZeroStateSuggestion* updated_suggestion1 =
      CreateSuggestion(@"Updated 1", @"Updated 1");
  ZeroStateSuggestion* updated_suggestion2 =
      CreateSuggestion(@"Updated 2", @"Updated 2");
  [view_controller_
      setZeroStateSuggestions:@[ updated_suggestion1, updated_suggestion2 ]];

  NSArray<UIButton*>* updated_buttons = FindButtons(view_controller_.view);
  ASSERT_EQ(updated_buttons.count, 2u);
  EXPECT_NSEQ(updated_buttons[0].configuration.title, @"Updated 1");
  EXPECT_NSEQ(updated_buttons[1].configuration.title, @"Updated 2");
}

// Tests that tapping a chip button notifies the mutator with the correct
// suggestion.
TEST_F(GeminiZeroStateViewControllerTest, TapChipButtonNotifiesMutator) {
  [view_controller_ loadViewIfNeeded];

  ZeroStateSuggestion* suggestion1 =
      CreateSuggestion(@"Summarize page", @"Summarize");
  ZeroStateSuggestion* suggestion2 =
      CreateSuggestion(@"Key takeaways", @"Takeaways");
  [view_controller_ setZeroStateSuggestions:@[ suggestion1, suggestion2 ]];

  NSArray<UIButton*>* buttons = FindButtons(view_controller_.view);
  ASSERT_EQ(buttons.count, 2u);

  OCMExpect([mock_mutator_ geminiZeroStateViewController:view_controller_
                                     didSelectSuggestion:suggestion2]);

  [buttons[1] sendActionsForControlEvents:UIControlEventTouchUpInside];

  id self = nil;
  OCMVerifyAll(mock_mutator_);
}

// Tests that tapping the close button notifies the mutator.
TEST_F(GeminiZeroStateViewControllerTest, TapCloseButtonNotifiesMutator) {
  [view_controller_ loadViewIfNeeded];

  UIButton* close_button = FindCloseButton(view_controller_.view);
  ASSERT_NE(close_button, nil);

  OCMExpect([mock_mutator_ didTapZeroStateCloseButton]);

  [close_button sendActionsForControlEvents:UIControlEventTouchUpInside];

  id self = nil;
  OCMVerifyAll(mock_mutator_);
}

// Tests that the greeting label displays the default text when the mutator
// provides no user first name.
TEST_F(GeminiZeroStateViewControllerTest, DefaultGreetingWithoutUserFirstName) {
  OCMStub([mock_mutator_ userFirstName]).andReturn(nil);

  [view_controller_ loadViewIfNeeded];

  NSArray<UILabel*>* labels =
      chrome_test_util::FindViewsByClass<UILabel>(view_controller_.view);
  ASSERT_GE(labels.count, 1u);
  EXPECT_NSEQ(labels[0].text, @"What do you want to work on?");
}

// Tests that the greeting label is personalized with the first name provided
// by the mutator when the view loads.
TEST_F(GeminiZeroStateViewControllerTest,
       GreetingWithUserFirstNameFromMutator) {
  OCMStub([mock_mutator_ userFirstName]).andReturn(@"Alex");

  [view_controller_ loadViewIfNeeded];

  NSArray<UILabel*>* labels =
      chrome_test_util::FindViewsByClass<UILabel>(view_controller_.view);
  ASSERT_GE(labels.count, 1u);
  EXPECT_NSEQ(labels[0].text, @"What do you want to work on, Alex?");
}

// Tests that a whitespace-only user first name falls back to the default
// greeting.
TEST_F(GeminiZeroStateViewControllerTest, GreetingWithWhitespaceUserFirstName) {
  OCMStub([mock_mutator_ userFirstName]).andReturn(@"   ");

  [view_controller_ loadViewIfNeeded];

  NSArray<UILabel*>* labels =
      chrome_test_util::FindViewsByClass<UILabel>(view_controller_.view);
  ASSERT_GE(labels.count, 1u);
  EXPECT_NSEQ(labels[0].text, @"What do you want to work on?");
}

// Tests that an empty user first name falls back to the default greeting.
TEST_F(GeminiZeroStateViewControllerTest, GreetingWithEmptyUserFirstName) {
  OCMStub([mock_mutator_ userFirstName]).andReturn(@"");

  [view_controller_ loadViewIfNeeded];

  NSArray<UILabel*>* labels =
      chrome_test_util::FindViewsByClass<UILabel>(view_controller_.view);
  ASSERT_GE(labels.count, 1u);
  EXPECT_NSEQ(labels[0].text, @"What do you want to work on?");
}

// Tests that suggestion chips maintain their intrinsic height and are not
// vertically stretched when the view controller's view has extra height.
TEST_F(GeminiZeroStateViewControllerTest,
       ChipsMaintainIntrinsicHeightWhenViewExpands) {
  ZeroStateSuggestion* suggestion1 =
      CreateSuggestion(@"Summarize page", @"Summarize");
  ZeroStateSuggestion* suggestion2 =
      CreateSuggestion(@"Key takeaways", @"Takeaways");
  [view_controller_ setZeroStateSuggestions:@[ suggestion1, suggestion2 ]];
  [view_controller_ loadViewIfNeeded];

  view_controller_.view.frame = CGRectMake(0, 0, 375, 600);
  [view_controller_.view layoutIfNeeded];

  NSArray<UIButton*>* buttons = FindButtons(view_controller_.view);
  ASSERT_EQ(buttons.count, 2u);

  // Both buttons should have intrinsic chip height and not be stretched to
  // fill the 600 pt height.
  EXPECT_NEAR(buttons[0].frame.size.height, buttons[1].frame.size.height, 1.0);
  EXPECT_LT(buttons[0].frame.size.height, 60.0);
  EXPECT_LT(buttons[1].frame.size.height, 60.0);
}
