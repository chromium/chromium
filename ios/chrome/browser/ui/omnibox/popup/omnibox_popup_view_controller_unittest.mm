// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_controller.h"

#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion_group_impl.h"
#import "ios/chrome/browser/ui/omnibox/popup/popup_match_preview_delegate.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class OmniboxPopupViewControllerTest : public PlatformTest {
 protected:
  void ExpectPreviewSuggestion(id suggestion, BOOL is_first_update) {
    [[preview_delegate_ expect] setPreviewSuggestion:suggestion
                                       isFirstUpdate:is_first_update];
  }
  BOOL PerformKeyboardAction(OmniboxKeyboardAction keyboardAction) {
    BOOL canPerform =
        [popup_view_controller_ canPerformKeyboardAction:keyboardAction];
    if (canPerform) {
      [popup_view_controller_ performKeyboardAction:keyboardAction];
    }
    return canPerform;
  }

  BOOL MoveHighlightUp() {
    return PerformKeyboardAction(OmniboxKeyboardActionUpArrow);
  }

  BOOL MoveHighlightDown() {
    return PerformKeyboardAction(OmniboxKeyboardActionDownArrow);
  }

  NSArray<id<AutocompleteSuggestion>>* GenerateMockSuggestions(
      NSUInteger nb_suggestions) {
    NSMutableArray* array =
        [[NSMutableArray alloc] initWithCapacity:nb_suggestions];
    for (NSUInteger i = 0; i < nb_suggestions; ++i) {
      id<AutocompleteSuggestion> mockSuggestion =
          OCMProtocolMock(@protocol(AutocompleteSuggestion));
      NSString* suggestionText =
          [NSString stringWithFormat:@"Suggestion %ld", i];
      OCMStub([mockSuggestion text])
          .andReturn(
              [[NSAttributedString alloc] initWithString:suggestionText]);
      [array addObject:mockSuggestion];
    }
    return array;
  }

  void SetUp() override {
    PlatformTest::SetUp();
    delegate_ = [OCMockObject
        mockForProtocol:@protocol(AutocompleteResultConsumerDelegate)];
    preview_delegate_ =
        [OCMockObject mockForProtocol:@protocol(PopupMatchPreviewDelegate)];
    return_delegate_ =
        [OCMockObject mockForProtocol:@protocol(OmniboxReturnDelegate)];
    popup_view_controller_ = [[OmniboxPopupViewController alloc] init];
    popup_view_controller_.delegate = delegate_;
    popup_view_controller_.matchPreviewDelegate = preview_delegate_;
    popup_view_controller_.acceptReturnDelegate = return_delegate_;
    // Force view initialisation since this view controller is never added into
    // the hierarchy in this unit test.
    [popup_view_controller_ view];

    first_suggestion_group_ = [AutocompleteSuggestionGroupImpl
        groupWithTitle:@""
           suggestions:GenerateMockSuggestions(5u)];
    second_suggestion_group_ = [AutocompleteSuggestionGroupImpl
        groupWithTitle:@""
           suggestions:GenerateMockSuggestions(10u)];
    suggestion_groups_ = @[ first_suggestion_group_, second_suggestion_group_ ];
  }

  OCMockObject<AutocompleteResultConsumerDelegate>* delegate_;
  OCMockObject<OmniboxReturnDelegate>* return_delegate_;
  OCMockObject<PopupMatchPreviewDelegate>* preview_delegate_;
  OmniboxPopupViewController* popup_view_controller_;

  id<AutocompleteSuggestionGroup> first_suggestion_group_;
  id<AutocompleteSuggestionGroup> second_suggestion_group_;
  NSArray<id<AutocompleteSuggestionGroup>>* suggestion_groups_;
};

// Tests that the first suggestion of the preselected group is highlighted on
// down arrow.
TEST_F(OmniboxPopupViewControllerTest,
       HighlightFirstSuggestionOfPreselectedGroup) {
  for (NSUInteger preselectedGroupIndex = 0;
       preselectedGroupIndex < suggestion_groups_.count;
       ++preselectedGroupIndex) {
    // Expect first suggestion of preselected group to be previewed.
    ExpectPreviewSuggestion(
        suggestion_groups_[preselectedGroupIndex].suggestions[0], YES);
    [popup_view_controller_ updateMatches:suggestion_groups_
               preselectedMatchGroupIndex:preselectedGroupIndex];
    [preview_delegate_ verify];

    // Expect first suggestion of the preselected group to be highlighted on
    // down arrow.
    ExpectPreviewSuggestion(
        suggestion_groups_[preselectedGroupIndex].suggestions[0], NO);
    EXPECT_TRUE(MoveHighlightDown());
    [preview_delegate_ verify];
  }
}

// Tests that highlighting with no suggestions do not preview any suggestions
// and does not crash.
TEST_F(OmniboxPopupViewControllerTest, UpDownArrowWithZeroMatches) {
  // Expect nil for preview suggestion.
  ExpectPreviewSuggestion(nil, YES);
  [popup_view_controller_ updateMatches:@[] preselectedMatchGroupIndex:0];
  [preview_delegate_ verify];

  [[[preview_delegate_ reject] ignoringNonObjectArgs]
      setPreviewSuggestion:OCMOCK_ANY
             isFirstUpdate:NO];

  // Up arrow key with zero suggestions should do nothing.
  EXPECT_TRUE(MoveHighlightUp());

  // Down arrow key with zero suggestions should do nothing.
  EXPECT_TRUE(MoveHighlightDown());
}

// Tests highlighting suggestions with one group of suggestions.
TEST_F(OmniboxPopupViewControllerTest, UpDownArrowWithOneGroup) {
  // Open match with one group of suggestions.
  ExpectPreviewSuggestion(first_suggestion_group_.suggestions[0], YES);
  [popup_view_controller_ updateMatches:@[ first_suggestion_group_ ]
             preselectedMatchGroupIndex:0];
  [preview_delegate_ verify];

  // Up Arrow when nothing is highlighted and when no group is above preselected
  // group, does nothing.
  EXPECT_TRUE(MoveHighlightUp());

  // Down Arrow highlights the next suggestion.
  for (NSUInteger i = 0; i < first_suggestion_group_.suggestions.count; ++i) {
    ExpectPreviewSuggestion(first_suggestion_group_.suggestions[i], NO);
    EXPECT_TRUE(MoveHighlightDown());
    [preview_delegate_ verify];
  }

  // Down Arrow when the last sugggestion is highlighted continues to highlight
  // the last suggestion.
  ExpectPreviewSuggestion(
      first_suggestion_group_
          .suggestions[first_suggestion_group_.suggestions.count - 1],
      NO);
  EXPECT_TRUE(MoveHighlightDown());
  [preview_delegate_ verify];

  // Up Arrow highlights the previous suggestion.
  for (NSInteger i = first_suggestion_group_.suggestions.count - 2; i >= 0;
       --i) {
    ExpectPreviewSuggestion(first_suggestion_group_.suggestions[i], NO);
    EXPECT_TRUE(MoveHighlightUp());
    [preview_delegate_ verify];
  }

  // Up Arrow when the first sugggestion is highlighted continues to highlight
  // the first suggestion.
  ExpectPreviewSuggestion(first_suggestion_group_.suggestions[0], NO);
  EXPECT_TRUE(MoveHighlightUp());
  [preview_delegate_ verify];
}

// Tests highlighting with two groups of suggestions.
TEST_F(OmniboxPopupViewControllerTest, UpDownArrowGroupSwitch) {
  // Open match with two groups of suggestions, with the second group as
  // preselected group.
  ExpectPreviewSuggestion(second_suggestion_group_.suggestions[0], YES);
  [popup_view_controller_ updateMatches:suggestion_groups_
             preselectedMatchGroupIndex:1];
  [preview_delegate_ verify];

  // Up Arrow when nothing is highlighted and with a group above the preselected
  // group, highlights the last suggestion of the group above.
  ExpectPreviewSuggestion(
      first_suggestion_group_
          .suggestions[first_suggestion_group_.suggestions.count - 1],
      NO);
  EXPECT_TRUE(MoveHighlightUp());
  [preview_delegate_ verify];

  // Down Arrow when the last suggestion of the first group is highlighted,
  // highlights the first suggestion of the second group.
  ExpectPreviewSuggestion(second_suggestion_group_.suggestions[0], NO);
  EXPECT_TRUE(MoveHighlightDown());
  [preview_delegate_ verify];

  // Up Arrow when the first suggestion of the second group is highlighted,
  // highlights the last suggestion of the first group.
  ExpectPreviewSuggestion(
      first_suggestion_group_
          .suggestions[first_suggestion_group_.suggestions.count - 1],
      NO);
  EXPECT_TRUE(MoveHighlightUp());
  [preview_delegate_ verify];
}

// Tests Return key interaction when a suggestion is highlighted and when no
// suggestions are highlighted.
TEST_F(OmniboxPopupViewControllerTest, ReturnHighlightedSuggestion) {
  // Open match with two groups of suggestions, with the second group as
  // preselected group.
  ExpectPreviewSuggestion(first_suggestion_group_.suggestions[0], YES);
  [popup_view_controller_ updateMatches:suggestion_groups_
             preselectedMatchGroupIndex:0];
  [preview_delegate_ verify];

  // Pressing return key when no suggestion is highlighted calls the
  // OmniboxReturnDelegate.
  [[return_delegate_ expect] omniboxReturnPressed:OCMOCK_ANY];
  [popup_view_controller_ omniboxReturnPressed:nil];
  [return_delegate_ verify];

  // Select first suggestion with down arrow.
  ExpectPreviewSuggestion(first_suggestion_group_.suggestions[0], NO);
  EXPECT_TRUE(MoveHighlightDown());
  [preview_delegate_ verify];

  // Pressing return key when a suggestion is highlighted call the
  // AutocompleteResultConsumerDelegate.
  [[delegate_ expect]
      autocompleteResultConsumer:popup_view_controller_
             didSelectSuggestion:first_suggestion_group_.suggestions[0]
                           inRow:0];
  [popup_view_controller_ omniboxReturnPressed:nil];
  [delegate_ verify];
}

}  // namespace
