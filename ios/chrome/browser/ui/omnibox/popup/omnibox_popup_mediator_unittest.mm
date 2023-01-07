// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_mediator.h"

#import <memory>
#import <vector>

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_result_consumer.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"
#import "ios/chrome/browser/ui/omnibox/popup/favicon_retriever.h"
#import "ios/chrome/browser/ui/omnibox/popup/image_retriever.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/window_open_disposition.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OmniboxPopupMediator ()

- (void)groupCurrentSuggestionsFrom:(NSUInteger)begin to:(NSUInteger)end;

@end

namespace {

// Mock of ImageDataFetcher class.
class MockImageDataFetcher : public image_fetcher::ImageDataFetcher {
 public:
  MockImageDataFetcher() : image_fetcher::ImageDataFetcher(nullptr) {}
};

// Mock of OmniboxPopupMediatorDelegate.
class MockOmniboxPopupMediatorDelegate : public OmniboxPopupMediatorDelegate {
 public:
  MOCK_METHOD(bool,
              IsStarredMatch,
              (const AutocompleteMatch& match),
              (const, override));
  MOCK_METHOD(void,
              OnMatchSelected,
              (const AutocompleteMatch& match,
               size_t row,
               WindowOpenDisposition disposition),
              (override));
  MOCK_METHOD(void,
              OnMatchSelectedForAppending,
              (const AutocompleteMatch& match),
              (override));
  MOCK_METHOD(void,
              OnMatchSelectedForDeletion,
              (const AutocompleteMatch& match),
              (override));
  MOCK_METHOD(void, OnScroll, (), (override));
};

// Structure to configure fake AutocompleteMatch for tests.
struct TestData {
  // Content, Description and URL.
  std::string url;
  // Relevance score.
  int relevance;
  // Allowed to be default match status.
  bool allowed_to_be_default_match;
  // Type of the match
  AutocompleteMatchType::Type type{AutocompleteMatchType::SEARCH_SUGGEST};
};

void PopulateAutocompleteMatch(const TestData& data, AutocompleteMatch* match) {
  match->contents = base::UTF8ToUTF16(data.url);
  match->description = base::UTF8ToUTF16(data.url);
  match->type = data.type;
  match->fill_into_edit = base::UTF8ToUTF16(data.url);
  match->destination_url = GURL("http://" + data.url);
  match->relevance = data.relevance;
  match->allowed_to_be_default_match = data.allowed_to_be_default_match;
}

void PopulateAutocompleteMatches(const TestData* data,
                                 size_t count,
                                 ACMatches& matches) {
  for (size_t i = 0; i < count; ++i) {
    AutocompleteMatch match;
    PopulateAutocompleteMatch(data[i], &match);
    matches.push_back(match);
  }
}

class OmniboxPopupMediatorTest : public PlatformTest {
 public:
  OmniboxPopupMediatorTest()
      : resultConsumerGroups_(),
        resultConsumerGroupIndex_(0),
        groupBySearchVSURLArguments_() {}

 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    std::unique_ptr<image_fetcher::ImageDataFetcher> mockImageDataFetcher =
        std::make_unique<MockImageDataFetcher>();
    mockResultConsumer_ =
        OCMProtocolMock(@protocol(AutocompleteResultConsumer));

    mediator_ = [[OmniboxPopupMediator alloc]
        initWithFetcher:std::move(mockImageDataFetcher)
          faviconLoader:nil
               delegate:&delegate_];
    mediator_.consumer = mockResultConsumer_;

    // Stubs call to AutocompleteResultConsumer::updateMatches and stores
    // arguments.
    OCMStub([[mockResultConsumer_ ignoringNonObjectArgs]
                             updateMatches:[OCMArg any]
                preselectedMatchGroupIndex:0])
        .andDo(^(NSInvocation* invocation) {
          __unsafe_unretained NSArray* suggestions;
          [invocation getArgument:&suggestions atIndex:2];
          resultConsumerGroups_ = suggestions;
          [invocation getArgument:&resultConsumerGroupIndex_ atIndex:3];
        });

    // Stubs call to OmniboxPopupMediator::groupCurrentSuggestionsFrom and
    // stores arguments.
    id partialMockMediator_ = OCMPartialMock(mediator_);
    OCMStub([[partialMockMediator_ ignoringNonObjectArgs]
                groupCurrentSuggestionsFrom:0
                                         to:0])
        .andDo(^(NSInvocation* invocation) {
          NSUInteger begin;
          NSUInteger end;
          [invocation getArgument:&begin atIndex:2];
          [invocation getArgument:&end atIndex:3];
          groupBySearchVSURLArguments_.push_back({begin, end});
        });
  }

  void SetVisibleSuggestionCount(NSUInteger visibleSuggestionCount) {
    OCMStub([mockResultConsumer_ newResultsAvailable])
        .andDo(^(NSInvocation* invocation) {
          [mediator_
              requestResultsWithVisibleSuggestionCount:visibleSuggestionCount];
        });
  }

  ACMatches GetAutocompleteMatches() {
    TestData data[] = {
        // url, relevance, can_be_default, type
        {"search1.com", 1000, true, AutocompleteMatchType::SEARCH_SUGGEST},
        {"url1.com", 900, false,
         AutocompleteMatchType::NAVSUGGEST_PERSONALIZED},
        {"search2.com", 800, false, AutocompleteMatchType::SEARCH_SUGGEST},
        {"url2.com", 700, false,
         AutocompleteMatchType::NAVSUGGEST_PERSONALIZED},
        {"search3.com", 600, false, AutocompleteMatchType::SEARCH_SUGGEST},
        {"url3.com", 500, false,
         AutocompleteMatchType::NAVSUGGEST_PERSONALIZED},
        {"search4.com", 400, false, AutocompleteMatchType::SEARCH_SUGGEST},
        {"url4.com", 300, false,
         AutocompleteMatchType::NAVSUGGEST_PERSONALIZED},
    };
    ACMatches matches;
    PopulateAutocompleteMatches(data, std::size(data), matches);
    return matches;
  }

  // Checks that groupBySearchVSURL is called with arguments `begin` and `end`.
  void ExpectGroupBySearchVSURL(NSUInteger index,
                                NSUInteger begin,
                                NSUInteger end) {
    EXPECT_GE(groupBySearchVSURLArguments_.size(), index);
    EXPECT_EQ(begin, std::get<0>(groupBySearchVSURLArguments_[index]));
    EXPECT_EQ(end, std::get<1>(groupBySearchVSURLArguments_[index]));
  }

  OmniboxPopupMediator* mediator_;
  MockOmniboxPopupMediatorDelegate delegate_;
  AutocompleteResult autocompleteResult_;
  id mockResultConsumer_;
  NSArray<id<AutocompleteSuggestionGroup>>* resultConsumerGroups_;
  NSInteger resultConsumerGroupIndex_;
  std::vector<std::tuple<size_t, size_t>> groupBySearchVSURLArguments_;
};

// Tests the mediator initalisation.
TEST_F(OmniboxPopupMediatorTest, Init) {
  EXPECT_TRUE(mediator_);
}

// Tests that update matches with no matches returns one suggestion group with
// zero suggestions.
TEST_F(OmniboxPopupMediatorTest, UpdateMatchesEmpty) {
  SetVisibleSuggestionCount(0);
  AutocompleteResult empty_results = AutocompleteResult();
  [mediator_ updateMatches:empty_results];
  EXPECT_EQ(1ul, resultConsumerGroups_.count);
  EXPECT_EQ(0ul,
            resultConsumerGroups_[resultConsumerGroupIndex_].suggestions.count);
}

// Tests that the number of suggestions matches the number of matches.
TEST_F(OmniboxPopupMediatorTest, UpdateMatchesCount) {
  SetVisibleSuggestionCount(0);
  AutocompleteResult results = AutocompleteResult();
  results.AppendMatches(GetAutocompleteMatches());
  [mediator_ updateMatches:results];
  EXPECT_EQ(1ul, resultConsumerGroups_.count);
  EXPECT_EQ(results.size(),
            resultConsumerGroups_[resultConsumerGroupIndex_].suggestions.count);
}

// Tests that the suggestions are sorted by SearchVSURL.
TEST_F(OmniboxPopupMediatorTest, UpdateMatchesSorting) {
  std::unique_ptr<base::test::ScopedFeatureList> feature_list =
      std::make_unique<base::test::ScopedFeatureList>();
  feature_list->InitAndDisableFeature(omnibox::kAdaptiveSuggestionsCount);
  SetVisibleSuggestionCount(0);

  AutocompleteResult results = AutocompleteResult();
  results.AppendMatches(GetAutocompleteMatches());
  [mediator_ updateMatches:results];
  EXPECT_EQ(1ul, resultConsumerGroups_.count);
  // Expect SearchVSURL skipping the first suggestion because its the omnibox's
  // content.
  ExpectGroupBySearchVSURL(0, 1, results.size());
}

// Tests that with adaptive suggestions, if all suggestions are visible, they
// are sorted by search VS URL.
TEST_F(OmniboxPopupMediatorTest, AdaptiveSuggestionsAllVisible) {
  std::unique_ptr<base::test::ScopedFeatureList> feature_list =
      std::make_unique<base::test::ScopedFeatureList>();
  feature_list->InitAndEnableFeature(omnibox::kAdaptiveSuggestionsCount);

  AutocompleteResult results = AutocompleteResult();
  results.AppendMatches(GetAutocompleteMatches());
  SetVisibleSuggestionCount(results.size());
  [mediator_ updateMatches:results];

  EXPECT_EQ(1ul, resultConsumerGroups_.count);
  // Expect SearchVSURL skipping the first suggestion because its the omnibox's
  // content.
  ExpectGroupBySearchVSURL(0, 1, results.size());
}

// Tests that with adaptive suggestions, if only part of the suggestions are
// visible, the first part is sorted by search VS URL.
TEST_F(OmniboxPopupMediatorTest, AdaptiveSuggestionsPartVisible) {
  std::unique_ptr<base::test::ScopedFeatureList> feature_list =
      std::make_unique<base::test::ScopedFeatureList>();
  feature_list->InitAndEnableFeature(omnibox::kAdaptiveSuggestionsCount);

  // Set Visible suggestion count.
  const NSUInteger visibleSuggestionCount = 5;
  SetVisibleSuggestionCount(visibleSuggestionCount);
  // Configure matches.
  AutocompleteResult results = AutocompleteResult();
  results.AppendMatches(GetAutocompleteMatches());
  // Call update matches on mediator.
  [mediator_ updateMatches:results];

  EXPECT_EQ(1ul, resultConsumerGroups_.count);
  // Expect SearchVSURL skipping the first suggestion because its the omnibox's
  // content.
  ExpectGroupBySearchVSURL(0, 1, visibleSuggestionCount);
  ExpectGroupBySearchVSURL(1, visibleSuggestionCount, results.size());
}

}  // namespace
