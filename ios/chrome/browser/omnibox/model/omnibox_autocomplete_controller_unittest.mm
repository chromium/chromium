// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"

#import "base/test/task_environment.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_match_test_util.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "components/omnibox/browser/fake_autocomplete_provider_client.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "components/omnibox/browser/omnibox_controller.h"
#import "components/omnibox/browser/test_omnibox_client.h"
#import "ios/chrome/browser/omnibox/model/omnibox_popup_controller.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using testing::AtMost;

namespace {

// A mock class for the AutocompleteController.
class MockAutocompleteController : public AutocompleteController {
 public:
  MockAutocompleteController()
      : AutocompleteController(
            std::make_unique<FakeAutocompleteProviderClient>(),
            0) {}
  MockAutocompleteController(const MockAutocompleteController&) = delete;
  MockAutocompleteController& operator=(const MockAutocompleteController&) =
      delete;
  ~MockAutocompleteController() override = default;

  void SetAutocompleteMatches(const ACMatches& matches) {
    AutocompleteResult& results =
        const_cast<AutocompleteResult&>(this->result());
    results.ClearMatches();
    results.AppendMatches(matches);
  }

  MOCK_METHOD(void,
              GroupSuggestionsBySearchVsURL,
              (size_t begin, size_t end),
              (override));
};

}  // namespace

class OmniboxAutocompleteControllerTest : public PlatformTest {
 public:
  OmniboxAutocompleteControllerTest() {
    auto omnibox_client = std::make_unique<TestOmniboxClient>();
    omnibox_controller_ = std::make_unique<OmniboxController>(
        /*view=*/nullptr, std::move(omnibox_client));

    auto autocomplete = std::make_unique<MockAutocompleteController>();
    autocomplete_controller_ = autocomplete.get();
    omnibox_controller_->SetAutocompleteControllerForTesting(
        std::move(autocomplete));

    controller_ = [[OmniboxAutocompleteController alloc]
        initWithOmniboxController:omnibox_controller_.get()];

    popup_ = [OCMockObject mockForClass:OmniboxPopupController.class];
    controller_.omniboxPopupController = popup_;
  }

  ~OmniboxAutocompleteControllerTest() override {
    [controller_ disconnect];
    autocomplete_controller_ = nullptr;
    omnibox_controller_ = nullptr;
    popup_ = nil;
  }

  ACMatches SampleMatches() const {
    return {CreateSearchMatch(u"Clear History"), CreateSearchMatch(u"search 1"),
            CreateSearchMatch(u"search 2"),
            CreateHistoryURLMatch(
                /*destination_url=*/"http://this-site-matches.com")};
  }

 protected:
  // Message loop for the main test thread.
  base::test::TaskEnvironment environment_;

  OmniboxAutocompleteController* controller_;
  raw_ptr<MockAutocompleteController> autocomplete_controller_;
  std::unique_ptr<OmniboxController> omnibox_controller_;
  id popup_;
};

// Tests that adding fake matches adds them to the results.
TEST_F(OmniboxAutocompleteControllerTest, AddFakeMatches) {
  ACMatches sample_matches = SampleMatches();
  autocomplete_controller_->SetAutocompleteMatches(sample_matches);
  EXPECT_EQ(autocomplete_controller_->result().size(), sample_matches.size());
}

// Tests requesting result when there are none still calls
// updateWithSortedResults.
TEST_F(OmniboxAutocompleteControllerTest, RequestResultEmpty) {
  OCMExpect(
      [popup_ updateWithSortedResults:autocomplete_controller_->result()]);
  [controller_ requestResultsWithVisibleSuggestionCount:0];
  EXPECT_OCMOCK_VERIFY(popup_);
}

// Tests requesting result with all of them visible.
TEST_F(OmniboxAutocompleteControllerTest, RequestResultsAllVisible) {
  autocomplete_controller_->SetAutocompleteMatches(SampleMatches());

  // Expect one group of suggestions.
  EXPECT_CALL(*autocomplete_controller_,
              GroupSuggestionsBySearchVsURL(
                  1, autocomplete_controller_->result().size()));

  OCMExpect(
      [popup_ updateWithSortedResults:autocomplete_controller_->result()]);

  // Request results with everything visible.
  [controller_ requestResultsWithVisibleSuggestionCount:0];

  EXPECT_OCMOCK_VERIFY(popup_);
}

// Tests requesting result with more suggestions visible than available.
TEST_F(OmniboxAutocompleteControllerTest, RequestResultVisibleOverflow) {
  autocomplete_controller_->SetAutocompleteMatches(SampleMatches());

  // Expect one group of suggestions.
  EXPECT_CALL(*autocomplete_controller_,
              GroupSuggestionsBySearchVsURL(
                  1, autocomplete_controller_->result().size()));

  OCMExpect(
      [popup_ updateWithSortedResults:autocomplete_controller_->result()]);

  // Request results with more visible than available.
  [controller_ requestResultsWithVisibleSuggestionCount:100];

  EXPECT_OCMOCK_VERIFY(popup_);
}

// Tests requesting result with part of them visible.
TEST_F(OmniboxAutocompleteControllerTest, RequestResultPartVisible) {
  autocomplete_controller_->SetAutocompleteMatches(SampleMatches());

  size_t result_size = autocomplete_controller_->result().size();
  size_t visible_count = 2;
  EXPECT_LT(visible_count, result_size);

  // Expect a first group of visible suggestions.
  EXPECT_CALL(*autocomplete_controller_,
              GroupSuggestionsBySearchVsURL(1, visible_count));

  // Expect a second group of hidden suggestions.
  EXPECT_CALL(*autocomplete_controller_,
              GroupSuggestionsBySearchVsURL(visible_count, result_size));

  OCMExpect(
      [popup_ updateWithSortedResults:autocomplete_controller_->result()]);

  // Request results with everything visible.
  [controller_ requestResultsWithVisibleSuggestionCount:visible_count];

  EXPECT_OCMOCK_VERIFY(popup_);
}
