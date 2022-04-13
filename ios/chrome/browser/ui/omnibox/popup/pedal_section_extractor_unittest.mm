// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/pedal_section_extractor.h"

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

class PedalSectionExtractorTest : public PlatformTest {
 protected:
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

  [[data_sink_ expect] updateMatches:@[ group ] withAnimation:NO];

  [extractor_ updateMatches:@[ group ] withAnimation:NO];

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

    EXPECT_EQ(groups.count, 2u);
    EXPECT_EQ(groups[0].suggestions.count, 1u);

    id<AutocompleteSuggestion> suggestion = groups[0].suggestions[0];
    EXPECT_EQ(suggestion.text.string, @"pedal title");
  };

  [[[data_sink_ stub] andDo:verifyGroups] updateMatches:[OCMArg any]
                                          withAnimation:NO];

  [extractor_ updateMatches:@[ group ] withAnimation:NO];

  [data_sink_ verify];
}

// After showing a pedal, the extractor will forget it exists when a fresh
// result with no pedals comes in.
TEST_F(PedalSectionExtractorTest, ResetsOnEachRun) {
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

  [[data_sink_ expect] updateMatches:[OCMArg any] withAnimation:NO];
  [extractor_ updateMatches:@[ group ] withAnimation:NO];
  [data_sink_ verify];

  AutocompleteSuggestionGroupImpl* groupNoPedals =
      [AutocompleteSuggestionGroupImpl
          groupWithTitle:@""
             suggestions:@[ mockSuggestionNoPedal ]];

  [[data_sink_ expect] updateMatches:@[ groupNoPedals ] withAnimation:NO];
  [extractor_ updateMatches:@[ groupNoPedals ] withAnimation:NO];
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

// Only the first 3 suggestions are considered for pedal extraction.
TEST_F(PedalSectionExtractorTest, OnlyExtractFirstFewRows) {
  id mockSuggestionNoPedal0 =
      [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
  [[[mockSuggestionNoPedal0 stub] andReturn:nil] pedal];
  id mockSuggestionNoPedal1 =
      [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
  [[[mockSuggestionNoPedal1 stub] andReturn:nil] pedal];
  id mockSuggestionNoPedal2 =
      [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
  [[[mockSuggestionNoPedal2 stub] andReturn:nil] pedal];

  id mockPedal = [OCMockObject mockForProtocol:@protocol(OmniboxPedal)];
  [[[mockPedal stub] andReturn:@"pedal title"] title];
  id mockSuggestionWithPedal =
      [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
  [[[mockSuggestionWithPedal stub] andReturn:mockPedal] pedal];

  AutocompleteSuggestionGroupImpl* group = [AutocompleteSuggestionGroupImpl
      groupWithTitle:@""
         suggestions:@[
           mockSuggestionNoPedal0, mockSuggestionNoPedal1,
           mockSuggestionNoPedal2, mockSuggestionWithPedal
         ]];

  void (^verifyGroups)(NSInvocation*) = ^(NSInvocation* invocation) {
    __unsafe_unretained NSArray<id<AutocompleteSuggestionGroup>>* groups = nil;
    [invocation getArgument:&groups atIndex:2];

    EXPECT_EQ(groups.count, 1u);
    EXPECT_EQ(groups[0].suggestions.count, 4u);
  };

  [[[data_sink_ stub] andDo:verifyGroups] updateMatches:[OCMArg any]
                                          withAnimation:NO];

  [extractor_ updateMatches:@[ group ] withAnimation:NO];

  [data_sink_ verify];
}

// Only extract one pedal when there are multiple.
TEST_F(PedalSectionExtractorTest, OnlyExtractOnePedal) {
  id mockSuggestionNoPedal =
      [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
  [[[mockSuggestionNoPedal stub] andReturn:nil] pedal];

  id mockPedal = [OCMockObject mockForProtocol:@protocol(OmniboxPedal)];
  [[[mockPedal stub] andReturn:@"pedal title"] title];
  id mockSuggestionWithPedal0 =
      [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
  [[[mockSuggestionWithPedal0 stub] andReturn:mockPedal] pedal];

  id mockSuggestionWithPedal1 =
      [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
  [[[mockSuggestionWithPedal1 stub] andReturn:mockPedal] pedal];

  AutocompleteSuggestionGroupImpl* group = [AutocompleteSuggestionGroupImpl
      groupWithTitle:@""
         suggestions:@[
           mockSuggestionNoPedal, mockSuggestionWithPedal0,
           mockSuggestionWithPedal1
         ]];

  void (^verifyGroups)(NSInvocation*) = ^(NSInvocation* invocation) {
    __unsafe_unretained NSArray<id<AutocompleteSuggestionGroup>>* groups = nil;
    [invocation getArgument:&groups atIndex:2];

    EXPECT_EQ(groups.count, 2u);
    EXPECT_EQ(groups[0].suggestions.count, 1u);
    EXPECT_EQ(groups[1].suggestions.count, 3u);
  };

  [[[data_sink_ stub] andDo:verifyGroups] updateMatches:[OCMArg any]
                                          withAnimation:NO];

  [extractor_ updateMatches:@[ group ] withAnimation:NO];

  [data_sink_ verify];
}

#pragma mark - highlight tests

// Tests in this class start with a pedal and a regular match.
class PedalSectionExtractorHighlightTest : public PedalSectionExtractorTest {
 protected:
  void SetUp() override {
    PedalSectionExtractorTest::SetUp();

    return_delegate_ =
        [OCMockObject mockForProtocol:@protocol(OmniboxReturnDelegate)];
    extractor_.acceptDelegate = return_delegate_;

    id mockSuggestionNoPedal =
        [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
    [[[mockSuggestionNoPedal stub] andReturn:nil] pedal];

    mock_pedal_ = [OCMockObject mockForProtocol:@protocol(OmniboxPedal)];
    [[[mock_pedal_ stub] andReturn:@"pedal title"] title];
    id mockSuggestionWithPedal =
        [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
    [[[mockSuggestionWithPedal stub] andReturn:mock_pedal_] pedal];

    AutocompleteSuggestionGroupImpl* group = [AutocompleteSuggestionGroupImpl
        groupWithTitle:@""
           suggestions:@[ mockSuggestionNoPedal, mockSuggestionWithPedal ]];

    [[data_sink_ expect] updateMatches:[OCMArg any] withAnimation:NO];
    [extractor_ updateMatches:@[ group ] withAnimation:NO];
  }

  OCMockObject<OmniboxPedal>* mock_pedal_;
  OCMockObject<OmniboxReturnDelegate>* return_delegate_;
};

// Highlighting is forwarded, except for when pedal is highlighted, in which
// case reproduce the highlighting side effects and pretend there's no
// highlight.
TEST_F(PedalSectionExtractorHighlightTest, highlightForwarding) {
  // When the first suggestion is highlighted in the popup, pretend there's no
  // pedal section at index 0.
  [[delegate_ expect] autocompleteResultConsumer:extractor_
                                 didHighlightRow:0
                                       inSection:0];
  [extractor_ autocompleteResultConsumer:nil didHighlightRow:0 inSection:1];
  [delegate_ verify];

  // Same for second suggestion
  [[delegate_ expect] autocompleteResultConsumer:extractor_
                                 didHighlightRow:1
                                       inSection:0];
  [extractor_ autocompleteResultConsumer:nil didHighlightRow:1 inSection:1];
  [delegate_ verify];

  // When the pedal is highlighted in the popup, pretend the highlighting has
  // went away. Then fake the highlighting by updating the match preview.
  [[preview_delegate_ expect]
      setPreviewMatchText:[[NSAttributedString alloc]
                              initWithString:@"pedal title"]
                    image:nil];
  [[delegate_ expect]
      autocompleteResultConsumerCancelledHighlighting:extractor_];
  [extractor_ autocompleteResultConsumer:nil didHighlightRow:0 inSection:0];
  [delegate_ verify];
  [preview_delegate_ verify];

  // When the highlighting is cancelled, forward this.
  [[delegate_ expect]
      autocompleteResultConsumerCancelledHighlighting:extractor_];
  [extractor_ autocompleteResultConsumerCancelledHighlighting:extractor_];
  [delegate_ verify];
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

// Return button is forwarded, unless a pedal is selected.
// When a pedal is selected, it's immediately executed.
TEST_F(PedalSectionExtractorHighlightTest, ReturnButton) {
  __block BOOL pedalTriggered = NO;
  void (^pedalBlock)(void) = ^() {
    pedalTriggered = YES;
  };

  [[[mock_pedal_ stub] andReturn:pedalBlock] action];

  // When the first suggestion is highlighted in the popup, and return is
  // pressed, the return is forwarded and the pedal is not triggered.
  [[delegate_ expect] autocompleteResultConsumer:extractor_
                                 didHighlightRow:0
                                       inSection:0];
  [extractor_ autocompleteResultConsumer:nil didHighlightRow:0 inSection:1];
  [[return_delegate_ expect] omniboxReturnPressed:nil];
  [extractor_ omniboxReturnPressed:nil];
  [delegate_ verify];
  [return_delegate_ verify];
  EXPECT_EQ(pedalTriggered, NO);

  // Same for second suggestion
  [[delegate_ expect] autocompleteResultConsumer:extractor_
                                 didHighlightRow:1
                                       inSection:0];
  [extractor_ autocompleteResultConsumer:nil didHighlightRow:1 inSection:1];
  [[return_delegate_ expect] omniboxReturnPressed:nil];
  [extractor_ omniboxReturnPressed:nil];
  [delegate_ verify];
  [return_delegate_ verify];
  EXPECT_EQ(pedalTriggered, NO);

  // Highlight the pedal
  [[preview_delegate_ expect]
      setPreviewMatchText:[[NSAttributedString alloc]
                              initWithString:@"pedal title"]
                    image:nil];
  [[delegate_ expect]
      autocompleteResultConsumerCancelledHighlighting:extractor_];
  [extractor_ autocompleteResultConsumer:nil didHighlightRow:0 inSection:0];
  [delegate_ verify];
  [preview_delegate_ verify];

  // Cancel highlighting and hit return. This should be forwarded.
  [[delegate_ expect]
      autocompleteResultConsumerCancelledHighlighting:extractor_];
  [extractor_ autocompleteResultConsumerCancelledHighlighting:extractor_];
  [delegate_ verify];
  [[return_delegate_ expect] omniboxReturnPressed:nil];
  [extractor_ omniboxReturnPressed:nil];
  [return_delegate_ verify];
  EXPECT_EQ(pedalTriggered, NO);

  // Highlight the pedal again.
  [[preview_delegate_ expect]
      setPreviewMatchText:[[NSAttributedString alloc]
                              initWithString:@"pedal title"]
                    image:nil];
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
