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
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "components/omnibox/browser/mock_autocomplete_provider_client.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "components/search_engines/search_engines_test_environment.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/template_url_service_client.h"
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

class OmniboxPopupMediatorTest : public PlatformTest {
 public:
  OmniboxPopupMediatorTest() {}

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
  }

  void TearDown() override { [mediator_ disconnect]; }


  // Message loop for the main test thread.
  base::test::TaskEnvironment environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  OmniboxPopupMediator* mediator_;
  MockOmniboxPopupMediatorDelegate delegate_;
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

}  // namespace
