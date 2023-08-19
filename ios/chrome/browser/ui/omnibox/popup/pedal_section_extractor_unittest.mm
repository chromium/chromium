// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/pedal_section_extractor.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion_group_impl.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_pedal.h"
#import "ios/chrome/browser/ui/omnibox/popup/popup_match_preview_delegate.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

using base::test::ios::WaitUntilConditionOrTimeout;

class PedalSectionExtractorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    extractor_ = [[PedalSectionExtractor alloc] init];
    delegate_ =
        [OCMockObject mockForProtocol:@protocol(PedalSectionExtractorDelegate)];
    extractor_.delegate = delegate_;

    mock_suggestion_no_pedal_ =
        [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
    [[[mock_suggestion_no_pedal_ stub] andReturn:nil] pedal];

    mock_pedal_ = [OCMockObject mockForProtocol:@protocol(OmniboxPedal)];
    [[[mock_pedal_ stub] andReturn:@"pedal title"] title];
    mock_suggestion_with_pedal_ =
        [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
    [[[mock_suggestion_with_pedal_ stub] andReturn:mock_pedal_] pedal];
  }

  PedalSectionExtractor* extractor_;
  OCMockObject<PedalSectionExtractorDelegate>* delegate_;
  OCMockObject<OmniboxPedal>* mock_pedal_;
  id mock_suggestion_no_pedal_;
  id mock_suggestion_with_pedal_;
};

// When there's no pedals, extractor returns nil.
TEST_F(PedalSectionExtractorTest, NoPedal) {
  id<AutocompleteSuggestionGroup> pedalGroup =
      [extractor_ extractPedals:@[ mock_suggestion_no_pedal_ ]];
  EXPECT_FALSE(pedalGroup);
}

// When there's a pedal, it's extracted into a "suggestion" with the same name
// in a separate group.
TEST_F(PedalSectionExtractorTest, ExtractPedals) {
  id<AutocompleteSuggestionGroup> pedalGroup = [extractor_ extractPedals:@[
    mock_suggestion_no_pedal_, mock_suggestion_with_pedal_
  ]];

  EXPECT_TRUE(pedalGroup);
  EXPECT_EQ(pedalGroup.suggestions.count, 1u);

  id<AutocompleteSuggestion> suggestion = pedalGroup.suggestions[0];
  EXPECT_EQ(suggestion.text.string, mock_pedal_.title);
}

// When a pedal disappears from the result list, the extractor prevents
// its disappearance for a short time to reduce UI updates.
TEST_F(PedalSectionExtractorTest, Debounce) {
  // Showing a result with pedals.
  {
    id<AutocompleteSuggestionGroup> pedalGroup = [extractor_ extractPedals:@[
      mock_suggestion_no_pedal_, mock_suggestion_with_pedal_
    ]];
    EXPECT_TRUE(pedalGroup);
    EXPECT_EQ(pedalGroup.suggestions.count, 1u);
  }

  // Updating with no pedals continues to return a pedal.
  {
    id<AutocompleteSuggestionGroup> pedalGroup =
        [extractor_ extractPedals:@[ mock_suggestion_no_pedal_ ]];
    EXPECT_TRUE(pedalGroup);
    EXPECT_EQ(pedalGroup.suggestions.count, 1u);
  }

  // Expect pedal removal when debounce timer expires
  [[delegate_ expect] invalidatePedals];

  // Verify that the pedal cache expires.
  ASSERT_TRUE(WaitUntilConditionOrTimeout(base::Seconds(1), ^{
    return ![extractor_ hasCachedPedals];
  }));

  [delegate_ verify];

  // Now updating from no pedals to no pedals, nothing happens
  [[delegate_ reject] invalidatePedals];
  {
    id<AutocompleteSuggestionGroup> pedalGroup =
        [extractor_ extractPedals:@[ mock_suggestion_no_pedal_ ]];
    EXPECT_FALSE(pedalGroup);
  }
  [delegate_ verify];

  // Since there's no update, nothing happens after the debounce timer expires
  // again.
  EXPECT_FALSE([extractor_ hasCachedPedals]);
}

// When the list of suggestions is completely empty, prevent the pedal from
// sticking around. This should only happen when the popup is being closed,
// otherwise there's at least a what-you-type suggestion.
TEST_F(PedalSectionExtractorTest, DontDebounceEmptyList) {
  // Showing a result with pedals.
  {
    id<AutocompleteSuggestionGroup> pedalGroup = [extractor_ extractPedals:@[
      mock_suggestion_no_pedal_, mock_suggestion_with_pedal_
    ]];
    EXPECT_TRUE(pedalGroup);
    EXPECT_EQ(pedalGroup.suggestions.count, 1u);
  }

  // Updating with no pedals continues to return a pedal.
  {
    id<AutocompleteSuggestionGroup> pedalGroup =
        [extractor_ extractPedals:@[ mock_suggestion_no_pedal_ ]];
    EXPECT_TRUE(pedalGroup);
    EXPECT_EQ(pedalGroup.suggestions.count, 1u);
  }

  // Close popup. Pedal should be removed from cache.
  {
    [[delegate_ expect] invalidatePedals];
    id<AutocompleteSuggestionGroup> pedalGroup = [extractor_ extractPedals:@[]];
    EXPECT_FALSE(pedalGroup);
    [delegate_ verify];
  }

  // Open popup with no pedals, no pedal should show.
  {
    id<AutocompleteSuggestionGroup> pedalGroup =
        [extractor_ extractPedals:@[ mock_suggestion_no_pedal_ ]];
    EXPECT_FALSE(pedalGroup);
  }
}

}  // namespace
