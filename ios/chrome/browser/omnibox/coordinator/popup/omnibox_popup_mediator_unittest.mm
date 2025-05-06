// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/coordinator/popup/omnibox_popup_mediator.h"

#import <memory>
#import <vector>

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/omnibox/browser/actions/omnibox_action_in_suggest.h"
#import "components/omnibox/browser/actions/omnibox_pedal_concepts.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_match_test_util.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "components/omnibox/browser/fake_autocomplete_provider_client.h"
#import "components/omnibox/browser/mock_autocomplete_provider_client.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "components/search_engines/search_engines_test_environment.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/template_url_service_client.h"
#import "ios/chrome/browser/omnibox/coordinator/popup/omnibox_popup_mediator+Testing.h"
#import "ios/chrome/browser/omnibox/model/autocomplete_match_formatter.h"
#import "ios/chrome/browser/omnibox/model/autocomplete_result_wrapper.h"
#import "ios/chrome/browser/omnibox/model/autocomplete_suggestion.h"
#import "ios/chrome/browser/omnibox/model/autocomplete_suggestion_group_impl.h"
#import "ios/chrome/browser/omnibox/model/omnibox_image_fetcher.h"
#import "ios/chrome/browser/omnibox/model/omnibox_pedal.h"
#import "ios/chrome/browser/omnibox/model/omnibox_pedal_swift.h"
#import "ios/chrome/browser/omnibox/model/suggest_action.h"
#import "ios/chrome/browser/omnibox/ui/popup/omnibox_popup_consumer.h"
#import "ios/chrome/browser/omnibox/ui/popup/row/favicon_retriever.h"
#import "ios/chrome/browser/omnibox/ui/popup/row/image_retriever.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
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

// Returns an autocomplete suggestion with a reviews action attached to it.
id<AutocompleteSuggestion> SuggestionWithReviewsAction() {
  AutocompleteMatch actionMatch = CreateActionInSuggestMatch(
      u"Action", {omnibox::ActionInfo_ActionType_REVIEWS});

  AutocompleteMatchFormatter* suggestion =
      [[AutocompleteMatchFormatter alloc] initWithMatch:actionMatch];

  NSMutableArray* actions = [[NSMutableArray alloc] init];

  for (auto& action : actionMatch.actions) {
    SuggestAction* suggestAction =
        [SuggestAction actionWithOmniboxAction:action.get()];
    [actions addObject:suggestAction];
  }

  suggestion.actionsInSuggest = actions;

  return suggestion;
}

id<OmniboxPedal> OmniboxPedal(OmniboxPedalId pedalId) {
  return [[OmniboxPedalData alloc] initWithTitle:@""
                                        subtitle:@""
                               accessibilityHint:@""
                                           image:[[UIImage alloc] init]
                                  imageTintColor:nil
                                 backgroundColor:nil
                                imageBorderColor:nil
                                            type:static_cast<int>(pedalId)
                                          action:^{
                                          }];
}

// Mock of ImageDataFetcher class.
class MockImageDataFetcher : public image_fetcher::ImageDataFetcher {
 public:
  MockImageDataFetcher() : image_fetcher::ImageDataFetcher(nullptr) {}
};

class OmniboxPopupMediatorTest : public PlatformTest {
 public:
  OmniboxPopupMediatorTest() {}

 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    std::unique_ptr<image_fetcher::ImageDataFetcher> mock_image_data_fetcher =
        std::make_unique<MockImageDataFetcher>();

    feature_engagement::test::MockTracker tracker;

    mockResultConsumer_ = OCMProtocolMock(@protocol(OmniboxPopupConsumer));

    omnibox_image_fetcher_ = [[OmniboxImageFetcher alloc]
        initWithFaviconLoader:nil
                 imageFetcher:std::move(mock_image_data_fetcher)];

    mediator_ =
        [[OmniboxPopupMediator alloc] initWithTracker:&tracker
                                  omniboxImageFetcher:omnibox_image_fetcher_];
    mediator_.consumer = mockResultConsumer_;
  }

  // Message loop for the main test thread.
  base::test::TaskEnvironment environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  OmniboxPopupMediator* mediator_;
  OmniboxImageFetcher* omnibox_image_fetcher_;
  id mockResultConsumer_;
};

// Tests the mediator initalisation.
TEST_F(OmniboxPopupMediatorTest, Init) {
  EXPECT_TRUE(mediator_);
}

// Tests that the right "PasswordManager.ManagePasswordsReferrer" metric is
// recorded when tapping the Manage Passwords suggestion.
TEST_F(OmniboxPopupMediatorTest, SelectManagePasswordSuggestionMetricLogged) {
  id<OmniboxPedal> pedal = OmniboxPedal(OmniboxPedalId::MANAGE_PASSWORDS);
  id mockSuggestionWithPedal =
      [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
  [[[mockSuggestionWithPedal stub] andReturn:pedal] pedal];
  [[[mockSuggestionWithPedal stub] andReturn:nil] actionsInSuggest];

  base::HistogramTester histogram_tester;

  // Verify that bucker count is zero.
  histogram_tester.ExpectBucketCount(
      "PasswordManager.ManagePasswordsReferrer",
      password_manager::ManagePasswordsReferrer::kOmniboxPedalSuggestion, 0);

  [mediator_ selectSuggestion:mockSuggestionWithPedal inRow:0];

  // Bucket count should now be one.
  histogram_tester.ExpectBucketCount(
      "PasswordManager.ManagePasswordsReferrer",
      password_manager::ManagePasswordsReferrer::kOmniboxPedalSuggestion, 1);
}

// Tests action in suggestion shown logged when selecting a non-action
// suggestion.
TEST_F(OmniboxPopupMediatorTest, ActionInSuggestMetricLogged) {
  id<AutocompleteSuggestion> match1 = SuggestionWithReviewsAction();
  id<AutocompleteSuggestion> match2 = [[AutocompleteMatchFormatter alloc]
      initWithMatch:CreateSearchMatch(u"search 1")];

  NSArray<id<AutocompleteSuggestion>>* mockedSuggestions =
      [[NSArray alloc] initWithObjects:match1, match2, nil];

  id<AutocompleteSuggestionGroup> group = [AutocompleteSuggestionGroupImpl
      groupWithTitle:@""
         suggestions:mockedSuggestions
                type:SuggestionGroupType::kUnspecifiedSuggestionGroup];

  NSArray<id<AutocompleteSuggestionGroup>>* groups =
      [[NSArray alloc] initWithObjects:group, nil];

  [mediator_ omniboxAutocompleteController:nil
                didUpdateSuggestionsGroups:groups];

  id<AutocompleteSuggestion> actionSuggestion =
      mediator_.suggestionGroups[0].suggestions[0];
  EXPECT_EQ(actionSuggestion.actionsInSuggest.count, 1u);

  id<AutocompleteSuggestion> nonActionSuggestion =
      mediator_.suggestionGroups[0].suggestions[1];
  EXPECT_EQ(nonActionSuggestion.actionsInSuggest.count, 0u);

  // Review type from ActionInSuggestType enum.
  const int kActionTypeReview = 4;

  base::HistogramTester histogram_tester;

  histogram_tester.ExpectBucketCount("Omnibox.ActionInSuggest.Shown",
                                     kActionTypeReview, 0);

  // Select an action suggestion.
  [mediator_ selectSuggestion:actionSuggestion inRow:0];

  // Expect Shown not logged when selecting an action.
  histogram_tester.ExpectBucketCount("Omnibox.ActionInSuggest.Shown",
                                     kActionTypeReview, 0);

  // Select another suggestion.
  [mediator_ selectSuggestion:nonActionSuggestion inRow:1];

  // Expect Shown logged.
  histogram_tester.ExpectBucketCount("Omnibox.ActionInSuggest.Shown",
                                     kActionTypeReview, 1);
}

// Tests pedals shown logged.
TEST_F(OmniboxPopupMediatorTest, PedalMetricLogged) {
  id<OmniboxPedal> pedal = OmniboxPedal(OmniboxPedalId::CLEAR_BROWSING_DATA);
  id match1 = [OCMockObject mockForProtocol:@protocol(AutocompleteSuggestion)];
  [[[match1 stub] andReturn:pedal] pedal];
  [[[match1 stub] andReturn:nil] actionsInSuggest];

  id<AutocompleteSuggestion> match2 = [[AutocompleteMatchFormatter alloc]
      initWithMatch:CreateSearchMatch(u"search 1")];

  id<AutocompleteSuggestionGroup> pedalgroup = [AutocompleteSuggestionGroupImpl
      groupWithTitle:@""
         suggestions:[[NSArray alloc] initWithObjects:match1, nil]
                type:SuggestionGroupType::kPedalSuggestionGroup];

  id<AutocompleteSuggestionGroup> nonPedalgroup =
      [AutocompleteSuggestionGroupImpl
          groupWithTitle:@""
             suggestions:[[NSArray alloc] initWithObjects:match2, nil]
                    type:SuggestionGroupType::kUnspecifiedSuggestionGroup];

  NSArray<id<AutocompleteSuggestionGroup>>* groups =
      [[NSArray alloc] initWithObjects:pedalgroup, nonPedalgroup, nil];

  [mediator_ omniboxAutocompleteController:nil
                didUpdateSuggestionsGroups:groups];

  base::HistogramTester histogram_tester;

  // Select a suggestion.
  [mediator_ selectSuggestion:match2 inRow:1];

  histogram_tester.ExpectUniqueSample("Omnibox.PedalShown", 1, 1);
}

}  // namespace
