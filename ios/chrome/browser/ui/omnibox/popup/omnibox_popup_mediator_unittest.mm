// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_mediator.h"

#import <memory>
#import <vector>

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "components/omnibox/browser/mock_autocomplete_provider_client.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "components/search_engines/search_engines_test_environment.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/template_url_service_client.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_result_consumer.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"
#import "ios/chrome/browser/ui/omnibox/popup/favicon_retriever.h"
#import "ios/chrome/browser/ui/omnibox/popup/image_retriever.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_mediator+Testing.h"
#import "ios/chrome/browser/ui/omnibox/popup/pedal_suggestion_wrapper.h"
#import "ios/chrome/browser/ui/omnibox/popup/popup_swift.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/window_open_disposition.h"
#import "url/gurl.h"

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
  MOCK_METHOD(void, OnCallActionTap, (), (override));
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
      : autocomplete_result_(),
        resultConsumerGroups_(),
        resultConsumerGroupIndex_(0),
        groupBySearchVSURLArguments_() {}

 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    // Setup for AutocompleteController.
    auto client = std::make_unique<MockAutocompleteProviderClient>();
    client->set_template_url_service(
        search_engines_test_environment_.template_url_service());
    auto autocomplete_controller =
        std::make_unique<testing::StrictMock<AutocompleteController>>(
            std::move(client), 0);

    std::unique_ptr<image_fetcher::ImageDataFetcher> mock_image_data_fetcher =
        std::make_unique<MockImageDataFetcher>();

    feature_engagement::test::MockTracker tracker;

    mockResultConsumer_ =
        OCMProtocolMock(@protocol(AutocompleteResultConsumer));

    mediator_ = [[OmniboxPopupMediator alloc]
                 initWithFetcher:std::move(mock_image_data_fetcher)
                   faviconLoader:nil
          autocompleteController:autocomplete_controller.get()
        remoteSuggestionsService:nil
                        delegate:&delegate_
                         tracker:&tracker];
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

    id partialMockMediator_ = OCMPartialMock(mediator_);
    // Stubs call to OmniboxPopupMediator::groupCurrentSuggestionsFrom and
    // stores arguments.
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

    // Stubs call to `autocompleteResult`.
    OCMStub([partialMockMediator_ autocompleteResult])
        .andReturn(&autocomplete_result_);
  }

  void TearDown() override { [mediator_ disconnect]; }

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

  // Message loop for the main test thread.
  base::test::TaskEnvironment environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  OmniboxPopupMediator* mediator_;
  MockOmniboxPopupMediatorDelegate delegate_;
  AutocompleteResult autocomplete_result_;
  id mockResultConsumer_;
  NSArray<id<AutocompleteSuggestionGroup>>* resultConsumerGroups_;
  NSInteger resultConsumerGroupIndex_;
  std::vector<std::tuple<size_t, size_t>> groupBySearchVSURLArguments_;
};

// Tests the mediator initalisation.
TEST_F(OmniboxPopupMediatorTest, Init) {
  EXPECT_TRUE(mediator_);
}

// Tests that update matches with no matches returns no suggestion groups.
TEST_F(OmniboxPopupMediatorTest, UpdateMatchesEmpty) {
  SetVisibleSuggestionCount(0);
  [mediator_ updateMatches:autocomplete_result_];
  EXPECT_EQ(0ul, resultConsumerGroups_.count);
}

// Tests that the number of suggestions matches the number of matches.
TEST_F(OmniboxPopupMediatorTest, UpdateMatchesCount) {
  SetVisibleSuggestionCount(0);
  autocomplete_result_.AppendMatches(GetAutocompleteMatches());
  [mediator_ updateMatches:autocomplete_result_];
  EXPECT_EQ(1ul, resultConsumerGroups_.count);
  EXPECT_EQ(autocomplete_result_.size(),
            resultConsumerGroups_[resultConsumerGroupIndex_].suggestions.count);
}

// Tests that if all suggestions are visible, they are sorted by search VS URL.
TEST_F(OmniboxPopupMediatorTest, SuggestionsAllVisible) {
  autocomplete_result_.AppendMatches(GetAutocompleteMatches());
  SetVisibleSuggestionCount(autocomplete_result_.size());
  [mediator_ updateMatches:autocomplete_result_];

  EXPECT_EQ(1ul, resultConsumerGroups_.count);
  // Expect SearchVSURL skipping the first suggestion because its the omnibox's
  // content.
  ExpectGroupBySearchVSURL(0, 1, autocomplete_result_.size());
}

// Tests that if only part of the suggestions are visible, the first part is
// sorted by search VS URL.
TEST_F(OmniboxPopupMediatorTest, SuggestionsPartVisible) {
  // Set Visible suggestion count.
  const NSUInteger visibleSuggestionCount = 5;
  SetVisibleSuggestionCount(visibleSuggestionCount);
  // Configure matches.
  autocomplete_result_.AppendMatches(GetAutocompleteMatches());
  // Call update matches on mediator.
  [mediator_ updateMatches:autocomplete_result_];

  EXPECT_EQ(1ul, resultConsumerGroups_.count);
  // Expect SearchVSURL skipping the first suggestion because its the omnibox's
  // content.
  ExpectGroupBySearchVSURL(0, 1, visibleSuggestionCount);
  ExpectGroupBySearchVSURL(1, visibleSuggestionCount,
                           autocomplete_result_.size());
}

// Tests that the right "PasswordManager.ManagePasswordsReferrer" metric is
// recorded when tapping the Manage Passwords suggestion.
TEST_F(OmniboxPopupMediatorTest, SelectManagePasswordSuggestionMetricLogged) {
  PedalSuggestionWrapper* pedal_suggestion_wrapper = [[PedalSuggestionWrapper
      alloc]
      initWithPedal:[[OmniboxPedalData alloc]
                            initWithTitle:@""
                                 subtitle:@""
                        accessibilityHint:@""
                                    image:[[UIImage alloc] init]
                           imageTintColor:nil
                          backgroundColor:nil
                         imageBorderColor:nil
                                     type:static_cast<int>(
                                              OmniboxPedalId::MANAGE_PASSWORDS)
                                   action:^{
                                   }]];
  base::HistogramTester histogram_tester;

  // Verify that bucker count is zero.
  histogram_tester.ExpectBucketCount(
      "PasswordManager.ManagePasswordsReferrer",
      password_manager::ManagePasswordsReferrer::kOmniboxPedalSuggestion, 0);

  [mediator_ autocompleteResultConsumer:mockResultConsumer_
                    didSelectSuggestion:pedal_suggestion_wrapper
                                  inRow:0];

  // Bucket count should now be one.
  histogram_tester.ExpectBucketCount(
      "PasswordManager.ManagePasswordsReferrer",
      password_manager::ManagePasswordsReferrer::kOmniboxPedalSuggestion, 1);
}

}  // namespace
