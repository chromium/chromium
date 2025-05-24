// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/popup/omnibox_popup_view_controller.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/omnibox/model/autocomplete_suggestion_group_impl.h"
#import "ios/chrome/browser/omnibox/ui/popup/omnibox_popup_consumer.h"
#import "ios/chrome/browser/omnibox/ui/popup/omnibox_popup_mutator.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class OmniboxPopupViewControllerTest : public PlatformTest {
 protected:
  void ExpectPreviewSuggestion(id suggestion, BOOL is_first_update) {
    [[mutator_ expect] previewSuggestion:suggestion
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
    return PerformKeyboardAction(OmniboxKeyboardAction::kUpArrow);
  }

  BOOL MoveHighlightDown() {
    return PerformKeyboardAction(OmniboxKeyboardAction::kDownArrow);
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
    mutator_ = [OCMockObject mockForProtocol:@protocol(OmniboxPopupMutator)];
    popup_view_controller_ = [[OmniboxPopupViewController alloc] init];
    popup_view_controller_.mutator = mutator_;
    // Force view initialisation since this view controller is never added into
    // the hierarchy in this unit test.
    [popup_view_controller_ view];

    first_suggestion_group_ = [AutocompleteSuggestionGroupImpl
        groupWithTitle:@""
           suggestions:GenerateMockSuggestions(5u)
                  type:SuggestionGroupType::kUnspecifiedSuggestionGroup];
    second_suggestion_group_ = [AutocompleteSuggestionGroupImpl
        groupWithTitle:@""
           suggestions:GenerateMockSuggestions(10u)
                  type:SuggestionGroupType::kUnspecifiedSuggestionGroup];
    suggestion_groups_ = @[ first_suggestion_group_, second_suggestion_group_ ];
  }
  // Message loop for the main test thread.
  base::test::TaskEnvironment environment_;

  OCMockObject<OmniboxPopupMutator>* mutator_;
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
    [mutator_ verify];

    // Expect first suggestion of the preselected group to be highlighted on
    // down arrow.
    ExpectPreviewSuggestion(
        suggestion_groups_[preselectedGroupIndex].suggestions[0], NO);
    EXPECT_TRUE(MoveHighlightDown());
    [mutator_ verify];
  }
}

// Tests that highlighting with no suggestions do not preview any suggestions
// and does not crash.
TEST_F(OmniboxPopupViewControllerTest, UpDownArrowWithZeroMatches) {
  // Expect nil for preview suggestion.
  ExpectPreviewSuggestion(nil, YES);
  [popup_view_controller_ updateMatches:@[] preselectedMatchGroupIndex:0];
  [mutator_ verify];

  [[[mutator_ reject] ignoringNonObjectArgs] previewSuggestion:OCMOCK_ANY
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
  [mutator_ verify];

  // Up Arrow when nothing is highlighted and when no group is above preselected
  // group, does nothing.
  EXPECT_TRUE(MoveHighlightUp());

  // Down Arrow highlights the next suggestion.
  for (NSUInteger i = 0; i < first_suggestion_group_.suggestions.count; ++i) {
    ExpectPreviewSuggestion(first_suggestion_group_.suggestions[i], NO);
    EXPECT_TRUE(MoveHighlightDown());
    [mutator_ verify];
  }

  // Down Arrow when the last sugggestion is highlighted continues to highlight
  // the last suggestion.
  ExpectPreviewSuggestion(
      first_suggestion_group_
          .suggestions[first_suggestion_group_.suggestions.count - 1],
      NO);
  EXPECT_TRUE(MoveHighlightDown());
  [mutator_ verify];

  // Up Arrow highlights the previous suggestion.
  for (NSInteger i = first_suggestion_group_.suggestions.count - 2; i >= 0;
       --i) {
    ExpectPreviewSuggestion(first_suggestion_group_.suggestions[i], NO);
    EXPECT_TRUE(MoveHighlightUp());
    [mutator_ verify];
  }

  // Up Arrow when the first sugggestion is highlighted continues to highlight
  // the first suggestion.
  ExpectPreviewSuggestion(first_suggestion_group_.suggestions[0], NO);
  EXPECT_TRUE(MoveHighlightUp());
  [mutator_ verify];
}

// Tests highlighting with two groups of suggestions.
TEST_F(OmniboxPopupViewControllerTest, UpDownArrowGroupSwitch) {
  // Open match with two groups of suggestions, with the second group as
  // preselected group.
  ExpectPreviewSuggestion(second_suggestion_group_.suggestions[0], YES);
  [popup_view_controller_ updateMatches:suggestion_groups_
             preselectedMatchGroupIndex:1];
  [mutator_ verify];

  // Up Arrow when nothing is highlighted and with a group above the preselected
  // group, highlights the last suggestion of the group above.
  ExpectPreviewSuggestion(
      first_suggestion_group_
          .suggestions[first_suggestion_group_.suggestions.count - 1],
      NO);
  EXPECT_TRUE(MoveHighlightUp());
  [mutator_ verify];

  // Down Arrow when the last suggestion of the first group is highlighted,
  // highlights the first suggestion of the second group.
  ExpectPreviewSuggestion(second_suggestion_group_.suggestions[0], NO);
  EXPECT_TRUE(MoveHighlightDown());
  [mutator_ verify];

  // Up Arrow when the first suggestion of the second group is highlighted,
  // highlights the last suggestion of the first group.
  ExpectPreviewSuggestion(
      first_suggestion_group_
          .suggestions[first_suggestion_group_.suggestions.count - 1],
      NO);
  EXPECT_TRUE(MoveHighlightUp());
  [mutator_ verify];
}

// Tests Return key interaction when a suggestion is highlighted and when no
// suggestions are highlighted.
TEST_F(OmniboxPopupViewControllerTest, ReturnHighlightedSuggestion) {
  // Open match with two groups of suggestions, with the second group as
  // preselected group.
  ExpectPreviewSuggestion(first_suggestion_group_.suggestions[0], YES);
  [popup_view_controller_ updateMatches:suggestion_groups_
             preselectedMatchGroupIndex:0];
  [mutator_ verify];

  // Pressing return key when no suggestion is highlighted.
  EXPECT_FALSE([popup_view_controller_
      canPerformKeyboardAction:OmniboxKeyboardAction::kReturnKey]);

  // Select first suggestion with down arrow.
  ExpectPreviewSuggestion(first_suggestion_group_.suggestions[0], NO);
  EXPECT_TRUE(MoveHighlightDown());
  [mutator_ verify];

  // Pressing return key when a suggestion is highlighted call the
  // mutator.
  [[mutator_ expect] selectSuggestion:first_suggestion_group_.suggestions[0]
                                inRow:0];
  EXPECT_TRUE([popup_view_controller_
      canPerformKeyboardAction:OmniboxKeyboardAction::kReturnKey]);
  [popup_view_controller_
      performKeyboardAction:OmniboxKeyboardAction::kReturnKey];
  [mutator_ verify];
}

}  // namespace
