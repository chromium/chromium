// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"

#import "base/functional/callback.h"
#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/omnibox/browser/autocomplete_classifier.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_match_test_util.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "components/omnibox/browser/fake_autocomplete_provider_client.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "components/omnibox/browser/test_omnibox_client.h"
#import "components/open_from_clipboard/fake_clipboard_recent_content.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller_delegate.h"
#import "ios/chrome/browser/omnibox/model/omnibox_controller_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_edit_model_ios.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/testing_application_context.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/gfx/image/image.h"
#import "ui/gfx/image/image_skia.h"
#import "ui/gfx/image/image_unittest_util.h"

using testing::AtMost;

namespace {

// A mock class for the AutocompleteController.
class MockAutocompleteController : public AutocompleteController {
 public:
  MockAutocompleteController()
      : AutocompleteController(
            std::make_unique<FakeAutocompleteProviderClient>(),
            AutocompleteClassifier::DefaultOmniboxProviders()) {}
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

  void SetSteadyStateOmniboxPosition(
      metrics::OmniboxEventProto::OmniboxPosition position) override {
    omnibox_position = position;
  }

  MOCK_METHOD(void,
              GroupSuggestionsBySearchVsURL,
              (size_t begin, size_t end),
              (override));

  metrics::OmniboxEventProto::OmniboxPosition omnibox_position;
};

/// A mock class for OmniboxEditModel.
class MockOmniboxEditModel : public OmniboxEditModelIOS {
 public:
  MockOmniboxEditModel(OmniboxControllerIOS* controller)
      : OmniboxEditModelIOS(controller, nullptr),
        last_opened_selection(OmniboxPopupSelection(UINT_MAX)) {}
  MockOmniboxEditModel(const MockOmniboxEditModel&) = delete;
  MockOmniboxEditModel& operator=(const MockOmniboxEditModel&) = delete;
  ~MockOmniboxEditModel() override = default;

  void OpenSelection(OmniboxPopupSelection selection,
                     base::TimeTicks timestamp,
                     WindowOpenDisposition disposition) override {
    last_opened_selection = selection;
    if (open_selection_closure) {
      open_selection_closure.Run();
      open_selection_closure.Reset();
    }
  }

  OmniboxPopupSelection last_opened_selection;
  base::RepeatingClosure open_selection_closure;
};

}  // namespace

class OmniboxAutocompleteControllerTest : public PlatformTest {
 public:
  OmniboxAutocompleteControllerTest() {
    auto clipboard = std::make_unique<FakeClipboardRecentContent>();
    clipboard_ = clipboard.get();
    ClipboardRecentContent::SetInstance(std::move(clipboard));

    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterLocalStatePrefs(local_state_->registry());
    TestingApplicationContext::GetGlobal()->SetLocalState(local_state_.get());

    auto omnibox_client = std::make_unique<TestOmniboxClient>();
    omnibox_controller_ = std::make_unique<OmniboxControllerIOS>(
        /*view=*/nullptr, std::move(omnibox_client));

    auto autocomplete = std::make_unique<MockAutocompleteController>();
    autocomplete_controller_ = autocomplete.get();
    omnibox_controller_->SetAutocompleteControllerForTesting(
        std::move(autocomplete));

    auto edit_model =
        std::make_unique<MockOmniboxEditModel>(omnibox_controller_.get());
    omnibox_edit_model_ = edit_model.get();
    omnibox_controller_->SetEditModelForTesting(std::move(edit_model));

    controller_delegate_ =
        OCMProtocolMock(@protocol(OmniboxAutocompleteControllerDelegate));

    controller_ = [[OmniboxAutocompleteController alloc]
        initWithOmniboxController:omnibox_controller_.get()];
    controller_.delegate = controller_delegate_;
  }

  ~OmniboxAutocompleteControllerTest() override {
    [controller_ disconnect];
    clipboard_ = nullptr;
    autocomplete_controller_ = nullptr;
    omnibox_edit_model_ = nullptr;
    omnibox_controller_ = nullptr;
    controller_delegate_ = nil;
    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
    local_state_.reset();
  }

  ACMatches SampleMatches() const {
    return {CreateSearchMatch(u"Clear History"), CreateSearchMatch(u"search 1"),
            CreateSearchMatch(u"search 2"),
            CreateHistoryURLMatch(
                /*destination_url=*/"http://this-site-matches.com")};
  }

  /// Returns the match opened by OmniboxEditModel::OpenSelection.
  const AutocompleteMatch& LastOpenedMatch() {
    return autocomplete_controller_->result().match_at(
        omnibox_edit_model_->last_opened_selection.line);
  }

 protected:
  // Message loop for the main test thread.
  base::test::TaskEnvironment environment_;
  // Application pref service.
  std::unique_ptr<TestingPrefServiceSimple> local_state_;

  OmniboxAutocompleteController* controller_;
  raw_ptr<MockAutocompleteController> autocomplete_controller_;
  raw_ptr<MockOmniboxEditModel> omnibox_edit_model_;
  raw_ptr<FakeClipboardRecentContent> clipboard_;
  std::unique_ptr<OmniboxControllerIOS> omnibox_controller_;
  id controller_delegate_;
};

// Custom matcher for AutocompleteMatch
MATCHER_P(IsSameAsMatch, expected, "") {
  return arg.destination_url == expected.destination_url &&
         arg.fill_into_edit == expected.fill_into_edit &&
         arg.additional_text == expected.additional_text &&
         arg.inline_autocompletion == expected.inline_autocompletion &&
         arg.contents == expected.contents &&
         arg.description == expected.description;
}

// Tests that adding fake matches adds them to the results.
TEST_F(OmniboxAutocompleteControllerTest, AddFakeMatches) {
  ACMatches sample_matches = SampleMatches();
  autocomplete_controller_->SetAutocompleteMatches(sample_matches);
  EXPECT_EQ(autocomplete_controller_->result().size(), sample_matches.size());
}

#pragma mark - Request suggestion

// Tests requesting result when there are none still calls
// the delegate to update the suggestions groups.
TEST_F(OmniboxAutocompleteControllerTest, RequestResultEmpty) {
  OCMExpect([controller_delegate_ omniboxAutocompleteController:[OCMArg any]
                                     didUpdateSuggestionsGroups:[OCMArg any]]);
  [controller_ requestSuggestionsWithVisibleSuggestionCount:0];

  EXPECT_OCMOCK_VERIFY(controller_delegate_);
}

// Tests requesting result with all of them visible.
TEST_F(OmniboxAutocompleteControllerTest, RequestResultsAllVisible) {
  autocomplete_controller_->SetAutocompleteMatches(SampleMatches());

  // Expect one group of suggestions.
  EXPECT_CALL(*autocomplete_controller_,
              GroupSuggestionsBySearchVsURL(
                  1, autocomplete_controller_->result().size()));

  OCMExpect([controller_delegate_ omniboxAutocompleteController:[OCMArg any]
                                     didUpdateSuggestionsGroups:[OCMArg any]]);

  // Request results with everything visible.
  [controller_ requestSuggestionsWithVisibleSuggestionCount:0];

  EXPECT_OCMOCK_VERIFY(controller_delegate_);
}

// Tests requesting result with more suggestions visible than available.
TEST_F(OmniboxAutocompleteControllerTest, RequestResultVisibleOverflow) {
  autocomplete_controller_->SetAutocompleteMatches(SampleMatches());

  // Expect one group of suggestions.
  EXPECT_CALL(*autocomplete_controller_,
              GroupSuggestionsBySearchVsURL(
                  1, autocomplete_controller_->result().size()));

  OCMExpect([controller_delegate_ omniboxAutocompleteController:[OCMArg any]
                                     didUpdateSuggestionsGroups:[OCMArg any]]);

  // Request results with more visible than available.
  [controller_ requestSuggestionsWithVisibleSuggestionCount:100];

  EXPECT_OCMOCK_VERIFY(controller_delegate_);
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

  OCMExpect([controller_delegate_ omniboxAutocompleteController:[OCMArg any]
                                     didUpdateSuggestionsGroups:[OCMArg any]]);

  // Request results with everything visible.
  [controller_ requestSuggestionsWithVisibleSuggestionCount:visible_count];

  EXPECT_OCMOCK_VERIFY(controller_delegate_);
}

#pragma mark - Logging

// Tests that omnibox position update is forwarded to autocompleteController.
TEST_F(OmniboxAutocompleteControllerTest, OmniboxPositionUpdates) {
  local_state_->SetBoolean(prefs::kBottomOmnibox, true);
  EXPECT_EQ(autocomplete_controller_->omnibox_position,
            metrics::OmniboxEventProto::BOTTOM_POSITION);

  local_state_->SetBoolean(prefs::kBottomOmnibox, false);
  EXPECT_EQ(autocomplete_controller_->omnibox_position,
            metrics::OmniboxEventProto::TOP_POSITION);
}

#pragma mark - Open match

// Tests opening a match that doesn't exist in autocomplete controller.
TEST_F(OmniboxAutocompleteControllerTest, OpenCreatedMatch) {
  autocomplete_controller_->SetAutocompleteMatches(SampleMatches());
  AutocompleteMatch match = CreateSearchMatch(u"some match");

  // Open match that doesn't come from the autocomplete controller. Row is
  // higher than autocomplete_controller_->result().size().
  [controller_ selectMatchForOpening:match
                               inRow:10
                              openIn:WindowOpenDisposition::CURRENT_TAB];

  // Expect the match to be opened.
  EXPECT_THAT(LastOpenedMatch(), IsSameAsMatch(match));

  // Reset the last opened selection.
  omnibox_edit_model_->last_opened_selection = OmniboxPopupSelection(UINT_MAX);

  // Open match that doesn't come from the autocomplete controller. Row is
  // smaller than autocomplete_controller_->result().size().
  [controller_ selectMatchForOpening:match
                               inRow:1
                              openIn:WindowOpenDisposition::CURRENT_TAB];
  // Expect the match to be opened.
  EXPECT_THAT(LastOpenedMatch(), IsSameAsMatch(match));
}

// Tests opening a clipboard URL match.
TEST_F(OmniboxAutocompleteControllerTest, OpenClipboardURLMatch) {
  // Create an empty clipboard match in autocompleteController.
  AutocompleteMatch clipboard_match = CreateAutocompleteMatch(
      "Clipboard match", AutocompleteMatchType::CLIPBOARD_URL, false, false,
      100, std::nullopt);
  clipboard_match.destination_url = GURL();
  autocomplete_controller_->SetAutocompleteMatches({clipboard_match});

  // Set the clipboard content.
  GURL pasteboard_url = GURL("https://chromium.org");
  clipboard_->SetClipboardURL(pasteboard_url, base::TimeDelta::Min());

  // Open the clipboard match.
  [controller_ selectMatchForOpening:clipboard_match
                               inRow:0
                              openIn:WindowOpenDisposition::CURRENT_TAB];

  // Expect the clipboard content to be loaded.
  EXPECT_EQ(LastOpenedMatch().destination_url, pasteboard_url);
}

// Tests opening a clipboard Text match.
TEST_F(OmniboxAutocompleteControllerTest, OpenClipboardTextMatch) {
  // Create an empty clipboard match in autocompleteController.
  AutocompleteMatch clipboard_match = CreateAutocompleteMatch(
      "Clipboard text match", AutocompleteMatchType::CLIPBOARD_TEXT, false,
      false, 100, std::nullopt);
  clipboard_match.destination_url = GURL();
  autocomplete_controller_->SetAutocompleteMatches({clipboard_match});

  // Set the clipboard content.
  std::u16string pasteboard_text = u"search terms";
  clipboard_->SetClipboardText(pasteboard_text, base::TimeDelta::Min());

  // Open the clipboard match.
  [controller_ selectMatchForOpening:clipboard_match
                               inRow:0
                              openIn:WindowOpenDisposition::CURRENT_TAB];

  // Expect the clipboard content to be loaded.
  EXPECT_EQ(LastOpenedMatch().fill_into_edit, pasteboard_text);
}

// Tests opening a clipboard Image match.
TEST_F(OmniboxAutocompleteControllerTest, OpenClipboardImageMatch) {
  // Create an empty clipboard match in autocompleteController.
  AutocompleteMatch clipboard_match = CreateAutocompleteMatch(
      "Clipboard image match", AutocompleteMatchType::CLIPBOARD_IMAGE, false,
      false, 100, std::nullopt);
  clipboard_match.destination_url = GURL();
  autocomplete_controller_->SetAutocompleteMatches({clipboard_match});

  // Set the clipboard content.
  gfx::Image pasteboard_image =
      gfx::test::CreateImage(/*width=*/10, /*height=*/10);
  clipboard_->SetClipboardImage(pasteboard_image, base::TimeDelta::Min());

  // Setup the OpenSelection waiter.
  base::RunLoop open_selection_waiter;
  omnibox_edit_model_->open_selection_closure =
      open_selection_waiter.QuitClosure();

  // Open the clipboard match.
  [controller_ selectMatchForOpening:clipboard_match
                               inRow:0
                              openIn:WindowOpenDisposition::CURRENT_TAB];

  // Wait for the image match.
  open_selection_waiter.Run();

  // Expect the clipboard content to be loaded.
  EXPECT_EQ(LastOpenedMatch().type, AutocompleteMatchType::CLIPBOARD_IMAGE);
  EXPECT_FALSE(LastOpenedMatch().post_content->first.empty());
  EXPECT_FALSE(LastOpenedMatch().post_content->second.empty());
}
