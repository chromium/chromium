// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/zero_state_suggestions/ui/gemini_zero_state_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/zero_state_suggestions/ui/gemini_zero_state_mutator.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/zero_state_suggestions_service.h"
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

// Recursively finds all UIButton instances in the view hierarchy.
NSArray<UIButton*>* FindButtons(UIView* view) {
  NSMutableArray<UIButton*>* buttons = [NSMutableArray array];
  if ([view isKindOfClass:[UIButton class]]) {
    [buttons addObject:static_cast<UIButton*>(view)];
  }
  for (UIView* subview in view.subviews) {
    [buttons addObjectsFromArray:FindButtons(subview)];
  }
  return buttons;
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

// Tests that the view hierarchy loads successfully.
TEST_F(GeminiZeroStateViewControllerTest, ViewDidLoad) {
  [view_controller_ loadViewIfNeeded];
  EXPECT_NE(view_controller_.view, nil);
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
