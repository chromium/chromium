// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui_bundled/popup/omnibox_popup_mediator.h"

#import <memory>
#import <vector>

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/omnibox/browser/actions/omnibox_action_in_suggest.h"
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
#import "ios/chrome/browser/omnibox/model/autocomplete_match_wrapper.h"
#import "ios/chrome/browser/omnibox/model/omnibox_popup_controller.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/autocomplete_result_consumer.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/autocomplete_suggestion.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/favicon_retriever.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/image_retriever.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/omnibox_popup_mediator+Testing.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/pedal_suggestion_wrapper.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/popup_swift.h"
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

    // Setup for AutocompleteController.
    autocomplete_controller_ = std::make_unique<AutocompleteController>(
        std::make_unique<FakeAutocompleteProviderClient>(), 0);

    std::unique_ptr<image_fetcher::ImageDataFetcher> mock_image_data_fetcher =
        std::make_unique<MockImageDataFetcher>();

    feature_engagement::test::MockTracker tracker;

    mockResultConsumer_ =
        OCMProtocolMock(@protocol(AutocompleteResultConsumer));

    mediator_ = [[OmniboxPopupMediator alloc]
                 initWithFetcher:std::move(mock_image_data_fetcher)
                   faviconLoader:nil
          autocompleteController:autocomplete_controller_.get()
        remoteSuggestionsService:nil
                         tracker:&tracker];
    mediator_.consumer = mockResultConsumer_;

    autocomplete_match_wrapper_ = [[AutocompleteMatchWrapper alloc] init];
    mediator_.autocompleteMatchWrapper = autocomplete_match_wrapper_;
  }

  void TearDown() override { [mediator_ disconnect]; }


  // Message loop for the main test thread.
  base::test::TaskEnvironment environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  OmniboxPopupMediator* mediator_;
  AutocompleteMatchWrapper* autocomplete_match_wrapper_;
  std::unique_ptr<AutocompleteController> autocomplete_controller_;
  id mockResultConsumer_;
};

// Tests the mediator initalisation.
TEST_F(OmniboxPopupMediatorTest, Init) {
  EXPECT_TRUE(mediator_);
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

// Tests action in suggestion shown logged when selecting a non-action
// suggestion.
TEST_F(OmniboxPopupMediatorTest, ActionInSuggestMetricLogged) {
  ACMatches matches = {CreateActionInSuggestMatch(
                           u"Action", {omnibox::ActionInfo_ActionType_REVIEWS}),
                       CreateSearchMatch(u"Clear History"),
                       CreateSearchMatch(u"search 1"),
                       CreateSearchMatch(u"search 2")};
  AutocompleteResult result = AutocompleteResult();
  result.AppendMatches(matches);
  [mediator_ popupController:nil didSortResults:result];

  id<AutocompleteSuggestion> actionSuggestion =
      mediator_.nonPedalSuggestions[0].suggestions[0];
  EXPECT_EQ(actionSuggestion.actionsInSuggest.count, 1u);

  id<AutocompleteSuggestion> nonActionSuggestion =
      mediator_.nonPedalSuggestions[0].suggestions[1];
  EXPECT_EQ(nonActionSuggestion.actionsInSuggest.count, 0u);

  // Review type from ActionInSuggestType enum.
  const int kActionTypeReview = 4;

  base::HistogramTester histogram_tester;

  histogram_tester.ExpectBucketCount("Omnibox.ActionInSuggest.Shown",
                                     kActionTypeReview, 0);

  // Select an action suggestion.
  [mediator_ autocompleteResultConsumer:nil
                    didSelectSuggestion:actionSuggestion
                                  inRow:0];

  // Expect Shown not logged when selecting an action.
  histogram_tester.ExpectBucketCount("Omnibox.ActionInSuggest.Shown",
                                     kActionTypeReview, 0);

  // Select another suggestion.
  [mediator_ autocompleteResultConsumer:nil
                    didSelectSuggestion:nonActionSuggestion
                                  inRow:1];

  // Expect Shown logged.
  histogram_tester.ExpectBucketCount("Omnibox.ActionInSuggest.Shown",
                                     kActionTypeReview, 1);
}

}  // namespace
