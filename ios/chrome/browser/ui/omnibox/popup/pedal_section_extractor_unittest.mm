// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/pedal_section_extractor.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion_group_impl.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_pedal.h"
#import "ios/chrome/browser/ui/omnibox/popup/popup_match_preview_delegate.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Waits without blocking the runloop.
void Wait(NSTimeInterval timeout) {
  NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:timeout];
  while ([[NSDate date] compare:deadline] != NSOrderedDescending) {
    base::test::ios::SpinRunLoopWithMaxDelay(base::Seconds(0.01));
  }
}

class PedalSectionExtractorTest : public PlatformTest {
 protected:
  void ExpectPreviewSuggestion(id suggestion, BOOL is_first_update) {
    [[preview_delegate_ expect] setPreviewSuggestion:suggestion
                                       isFirstUpdate:is_first_update];
  }

  void SetUp() override {
    PlatformTest::SetUp();
    extractor_ = [[PedalSectionExtractor alloc] init];
    data_sink_ =
        [OCMockObject mockForProtocol:@protocol(AutocompleteResultConsumer)];
    delegate_ = [OCMockObject
        mockForProtocol:@protocol(AutocompleteResultConsumerDelegate)];
    preview_delegate_ =
        [OCMockObject mockForProtocol:@protocol(PopupMatchPreviewDelegate)];

    extractor_.dataSink = data_sink_;
    extractor_.delegate = delegate_;
    extractor_.matchPreviewDelegate = preview_delegate_;
  }

  PedalSectionExtractor* extractor_;
  OCMockObject<AutocompleteResultConsumer>* data_sink_;
  OCMockObject<AutocompleteResultConsumerDelegate>* delegate_;
  OCMockObject<PopupMatchPreviewDelegate>* preview_delegate_;
};

// When there's no pedals, extractor doesn't change the result.
TEST_F(PedalSectionExtractorTest, ForwardsWhenNoPedals) {
  id mockSuggestion =
      [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
  [[[mockSuggestion expect] andReturn:nil] pedal];

  AutocompleteSuggestionGroupImpl* group =
      [AutocompleteSuggestionGroupImpl groupWithTitle:@""
                                          suggestions:@[ mockSuggestion ]];

  ExpectPreviewSuggestion(mockSuggestion, YES);
  [[data_sink_ expect] updateMatches:@[ group ] preselectedMatchGroupIndex:0];

  [extractor_ updateMatches:@[ group ] preselectedMatchGroupIndex:0];

  [data_sink_ verify];
}

// When there's a pedal, it's extracted into a "suggestion" with the same name
// in a separate group.
TEST_F(PedalSectionExtractorTest, ExtractsPedalsIntoSeparateSection) {
  id mockSuggestionNoPedal =
      [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
  [[[mockSuggestionNoPedal stub] andReturn:nil] pedal];

  id mockPedal = [OCMockObject mockForProtocol:@protocol(OmniboxPedal)];
  [[[mockPedal stub] andReturn:@"pedal title"] title];
  id mockSuggestionWithPedal =
      [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
  [[[mockSuggestionWithPedal stub] andReturn:mockPedal] pedal];

  AutocompleteSuggestionGroupImpl* group = [AutocompleteSuggestionGroupImpl
      groupWithTitle:@""
         suggestions:@[ mockSuggestionNoPedal, mockSuggestionWithPedal ]];

  void (^verifyGroups)(NSInvocation*) = ^(NSInvocation* invocation) {
    __unsafe_unretained NSArray<id<AutocompleteSuggestionGroup>>* groups = nil;
    [invocation getArgument:&groups atIndex:2];
    NSInteger preselectedMatchGroupIndex = -1;
    [invocation getArgument:&preselectedMatchGroupIndex atIndex:3];

    EXPECT_EQ(preselectedMatchGroupIndex, 1);
    EXPECT_EQ(groups.count, 2u);
    EXPECT_EQ(groups[0].suggestions.count, 1u);

    id<AutocompleteSuggestion> suggestion = groups[0].suggestions[0];
    EXPECT_EQ(suggestion.text.string, @"pedal title");
  };

  [[[data_sink_ stub] andDo:verifyGroups] updateMatches:[OCMArg any]
                             preselectedMatchGroupIndex:1];

  ExpectPreviewSuggestion(mockSuggestionNoPedal, YES);
  [extractor_ updateMatches:@[ group ] preselectedMatchGroupIndex:0];

  [data_sink_ verify];
}

// When a pedal disappears from the result list, the extractor prevents
// its disappearance for a short time to reduce UI updates.
TEST_F(PedalSectionExtractorTest, Debounce) {
  id mockSuggestionNoPedal =
      [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
  [[[mockSuggestionNoPedal stub] andReturn:nil] pedal];

  id mockPedal = [OCMockObject mockForProtocol:@protocol(OmniboxPedal)];
  [[[mockPedal stub] andReturn:@"pedal title"] title];
  id mockSuggestionWithPedal =
      [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
  [[[mockSuggestionWithPedal stub] andReturn:mockPedal] pedal];

  AutocompleteSuggestionGroupImpl* group = [AutocompleteSuggestionGroupImpl
      groupWithTitle:@""
         suggestions:@[ mockSuggestionNoPedal, mockSuggestionWithPedal ]];

  // Showing a result with pedals passes a pedal to the sink.

  [[data_sink_ expect] updateMatches:[OCMArg any] preselectedMatchGroupIndex:1];
  ExpectPreviewSuggestion(mockSuggestionNoPedal, YES);

  [extractor_ updateMatches:@[ group ] preselectedMatchGroupIndex:0];
  [data_sink_ verify];

  AutocompleteSuggestionGroupImpl* groupNoPedals =
      [AutocompleteSuggestionGroupImpl
          groupWithTitle:@""
             suggestions:@[ mockSuggestionNoPedal ]];

  // Updating with no pedals continues to pass a pedal to the sink.

  ExpectPreviewSuggestion(mockSuggestionNoPedal, YES);
  [[data_sink_ expect] updateMatches:[OCMArg any] preselectedMatchGroupIndex:1];
  [extractor_ updateMatches:@[ groupNoPedals ] preselectedMatchGroupIndex:0];

  [data_sink_ verify];

  // Expect pedal removal when debounce timer expires
  [[data_sink_ expect] updateMatches:@[ groupNoPedals ]
          preselectedMatchGroupIndex:0];

  // Wait for debounce to happen
  Wait(1);
  [data_sink_ verify];

  // Now updating from no pedals to no pedals, nothing happens
  [[data_sink_ expect] updateMatches:@[ groupNoPedals ]
          preselectedMatchGroupIndex:0];
  ExpectPreviewSuggestion(mockSuggestionNoPedal, YES);
  [extractor_ updateMatches:@[ groupNoPedals ] preselectedMatchGroupIndex:0];

  [data_sink_ verify];

  // Since there's no update, nothing happens after the debounce timer expires
  // again.
  Wait(1);
  [data_sink_ verify];
}

// When the list of suggestions is completely empty, prevent the pedal from
// sticking around. This should only happen when the popup is being closed,
// otherwise there's at least a what-you-type suggestion.
TEST_F(PedalSectionExtractorTest, DontDebounceEmptyList) {
  id mockSuggestionNoPedal =
      [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
  [[[mockSuggestionNoPedal stub] andReturn:nil] pedal];

  id mockPedal = [OCMockObject mockForProtocol:@protocol(OmniboxPedal)];
  [[[mockPedal stub] andReturn:@"pedal title"] title];
  id mockSuggestionWithPedal =
      [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
  [[[mockSuggestionWithPedal stub] andReturn:mockPedal] pedal];

  AutocompleteSuggestionGroupImpl* group = [AutocompleteSuggestionGroupImpl
      groupWithTitle:@""
         suggestions:@[ mockSuggestionNoPedal, mockSuggestionWithPedal ]];

  // Showing a result with pedals passes a pedal to the sink.

  [[data_sink_ expect] updateMatches:[OCMArg any] preselectedMatchGroupIndex:1];
  ExpectPreviewSuggestion(mockSuggestionNoPedal, YES);
  [extractor_ updateMatches:@[ group ] preselectedMatchGroupIndex:0];
  [data_sink_ verify];

  AutocompleteSuggestionGroupImpl* groupNoPedals =
      [AutocompleteSuggestionGroupImpl
          groupWithTitle:@""
             suggestions:@[ mockSuggestionNoPedal ]];

  // Updating with no pedals continues to pass a pedal to the sink.

  [[data_sink_ expect] updateMatches:[OCMArg any] preselectedMatchGroupIndex:1];
  ExpectPreviewSuggestion(mockSuggestionNoPedal, YES);
  [extractor_ updateMatches:@[ groupNoPedals ] preselectedMatchGroupIndex:0];

  [data_sink_ verify];

  // Expect pedal removal when debounce timer expires
  [[data_sink_ expect] updateMatches:@[ groupNoPedals ]
          preselectedMatchGroupIndex:0];

  // Wait for debounce to happen
  Wait(1);
  [data_sink_ verify];

  // Now updating from no pedals to no suggestions at all, the update goes
  // through.
  [[data_sink_ expect] updateMatches:@[] preselectedMatchGroupIndex:0];
  ExpectPreviewSuggestion(nil, YES);
  [extractor_ updateMatches:@[] preselectedMatchGroupIndex:0];

  [data_sink_ verify];

  // Since there's no update, nothing happens after the debounce timer expires
  // again.
  Wait(1);
  [data_sink_ verify];
}

// Forwards methods that are not affected by the pedal section.
TEST_F(PedalSectionExtractorTest, ForwardsIrrelevantMethods) {
  // consumer methods
  [[data_sink_ expect] setTextAlignment:NSTextAlignmentLeft];
  [extractor_ setTextAlignment:NSTextAlignmentLeft];
  [data_sink_ verify];

  [[data_sink_ expect]
      setSemanticContentAttribute:UISemanticContentAttributeSpatial];
  [extractor_ setSemanticContentAttribute:UISemanticContentAttributeSpatial];
  [data_sink_ verify];

  // delegate methods
  [[delegate_ expect] autocompleteResultConsumerDidScroll:extractor_];
  [extractor_ autocompleteResultConsumerDidScroll:nil];
  [delegate_ verify];
}

#pragma mark - highlight tests

// Tests in this class start with a pedal and a regular match.
class PedalSectionExtractorHighlightTest : public PedalSectionExtractorTest {
 protected:
  void ExpectPedalPreviewSuggestion(BOOL is_first_update) {
    void (^verifyPreviewMatch)(NSInvocation*) = ^(NSInvocation* invocation) {
      __unsafe_unretained id<AutocompleteSuggestion> match = nil;
      [invocation getArgument:&match atIndex:2];
      EXPECT_TRUE([match.text.string isEqualToString:@"pedal title"]);
    };

    [[[preview_delegate_ expect] andDo:verifyPreviewMatch]
        setPreviewSuggestion:[OCMArg any]
               isFirstUpdate:is_first_update];
  }

  void SetUp() override {
    PedalSectionExtractorTest::SetUp();

    return_delegate_ =
        [OCMockObject mockForProtocol:@protocol(OmniboxReturnDelegate)];
    extractor_.acceptDelegate = return_delegate_;

    mock_suggestion_no_pedal_ =
        [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
    [[[mock_suggestion_no_pedal_ stub] andReturn:nil] pedal];

    mock_pedal_ = [OCMockObject mockForProtocol:@protocol(OmniboxPedal)];
    [[[mock_pedal_ stub] andReturn:@"pedal title"] title];
    mock_suggestion_with_pedal_ =
        [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
    [[[mock_suggestion_with_pedal_ stub] andReturn:mock_pedal_] pedal];

    AutocompleteSuggestionGroupImpl* group = [AutocompleteSuggestionGroupImpl
        groupWithTitle:@""
           suggestions:@[
             mock_suggestion_no_pedal_, mock_suggestion_with_pedal_
           ]];

    [[data_sink_ expect] updateMatches:[OCMArg any]
            preselectedMatchGroupIndex:1];
    ExpectPreviewSuggestion(mock_suggestion_no_pedal_, YES);
    [extractor_ updateMatches:@[ group ] preselectedMatchGroupIndex:0];
    [preview_delegate_ verify];
  }

  OCMockObject<OmniboxPedal>* mock_pedal_;
  OCMockObject<OmniboxReturnDelegate>* return_delegate_;
  id mock_suggestion_no_pedal_;
  id mock_suggestion_with_pedal_;
};

// Test highlighting: highlighting sets the preview correctly, even for pedals.
TEST_F(PedalSectionExtractorHighlightTest, highlightsPedalsAndSuggestions) {
  // When the first suggestion is highlighted in the popup, pretend there's no
  // pedal section at index 0.
  ExpectPreviewSuggestion(mock_suggestion_no_pedal_, NO);
  [extractor_ autocompleteResultConsumer:nil didHighlightRow:0 inSection:1];
  [preview_delegate_ verify];

  // Same for second suggestion
  ExpectPreviewSuggestion(mock_suggestion_with_pedal_, NO);
  [extractor_ autocompleteResultConsumer:nil didHighlightRow:1 inSection:1];
  [preview_delegate_ verify];

  // When the pedal is highlighted in the popup, pretend the highlighting has
  // went away. Then fake the highlighting by updating the match preview.
  ExpectPedalPreviewSuggestion(NO);
  [[delegate_ expect]
      autocompleteResultConsumerCancelledHighlighting:extractor_];
  [extractor_ autocompleteResultConsumer:nil didHighlightRow:0 inSection:0];
  [preview_delegate_ verify];

  // When the highlighting is cancelled, forward this.
  ExpectPreviewSuggestion(nil, NO);
  [[delegate_ expect]
      autocompleteResultConsumerCancelledHighlighting:extractor_];
  [extractor_ autocompleteResultConsumerCancelledHighlighting:extractor_];
}

TEST_F(PedalSectionExtractorHighlightTest, ForwardsTrailingButton) {
  [[delegate_ expect] autocompleteResultConsumer:extractor_
                      didTapTrailingButtonForRow:0
                                       inSection:0];
  [extractor_ autocompleteResultConsumer:nil
              didTapTrailingButtonForRow:0
                               inSection:1];
  [delegate_ verify];
}

TEST_F(PedalSectionExtractorHighlightTest, ForwardsDeletion) {
  [[delegate_ expect] autocompleteResultConsumer:extractor_
                         didSelectRowForDeletion:0
                                       inSection:0];
  [extractor_ autocompleteResultConsumer:nil
                 didSelectRowForDeletion:0
                               inSection:1];
  [delegate_ verify];
}

// Return button is equivalent to selection, unless a pedal is selected.
// When a pedal is selected, it's immediately executed.
TEST_F(PedalSectionExtractorHighlightTest, ReturnButton) {
  __block BOOL pedalTriggered = NO;
  void (^pedalBlock)(void) = ^() {
    pedalTriggered = YES;
  };

  [[[mock_pedal_ stub] andReturn:pedalBlock] action];

  // When the first suggestion is highlighted in the popup, and return is
  // pressed, the delegate receives a selection event and the pedal is not
  // triggered.
  ExpectPreviewSuggestion(mock_suggestion_no_pedal_, NO);
  [extractor_ autocompleteResultConsumer:nil didHighlightRow:0 inSection:1];
  [[delegate_ expect] autocompleteResultConsumer:[OCMArg any]
                                    didSelectRow:0
                                       inSection:0];
  [extractor_ omniboxReturnPressed:nil];
  [delegate_ verify];
  [return_delegate_ verify];
  EXPECT_EQ(pedalTriggered, NO);

  // Same for second suggestion
  ExpectPreviewSuggestion(mock_suggestion_with_pedal_, NO);
  [[delegate_ expect] autocompleteResultConsumer:[OCMArg any]
                                    didSelectRow:1
                                       inSection:0];
  [extractor_ autocompleteResultConsumer:nil didHighlightRow:1 inSection:1];
  [extractor_ omniboxReturnPressed:nil];
  [delegate_ verify];
  [return_delegate_ verify];
  [preview_delegate_ verify];
  EXPECT_EQ(pedalTriggered, NO);

  // Highlight the pedal
  ExpectPedalPreviewSuggestion(NO);
  [[delegate_ expect]
      autocompleteResultConsumerCancelledHighlighting:extractor_];
  [extractor_ autocompleteResultConsumer:nil didHighlightRow:0 inSection:0];
  [delegate_ verify];
  [preview_delegate_ verify];

  // Cancel highlighting and hit return. This should be forwarded.
  ExpectPreviewSuggestion(nil, NO);
  [[delegate_ expect]
      autocompleteResultConsumerCancelledHighlighting:extractor_];
  [extractor_ autocompleteResultConsumerCancelledHighlighting:extractor_];
  [delegate_ verify];
  [[return_delegate_ expect] omniboxReturnPressed:nil];
  [extractor_ omniboxReturnPressed:nil];
  [return_delegate_ verify];
  EXPECT_EQ(pedalTriggered, NO);

  // Highlight the pedal again.
  ExpectPedalPreviewSuggestion(NO);
  [[delegate_ expect]
      autocompleteResultConsumerCancelledHighlighting:extractor_];
  [extractor_ autocompleteResultConsumer:nil didHighlightRow:0 inSection:0];
  [delegate_ verify];
  [preview_delegate_ verify];

  // Hit return. The pedal should be executed.
  [extractor_ omniboxReturnPressed:nil];
  EXPECT_EQ(pedalTriggered, YES);
}

}  // namespace
