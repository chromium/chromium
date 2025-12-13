// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"

#import "base/functional/callback.h"
#import "base/run_loop.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/omnibox/browser/autocomplete_classifier.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_controller_config.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_match_test_util.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "components/omnibox/browser/fake_autocomplete_provider_client.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "components/omnibox/browser/omnibox_popup_selection.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/omnibox/browser/search_provider.h"
#import "components/omnibox/browser/test_omnibox_client.h"
#import "components/open_from_clipboard/fake_clipboard_recent_content.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller+Testing.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller_delegate.h"
#import "ios/chrome/browser/omnibox/model/omnibox_metrics_recorder.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_model.h"
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

using testing::_;
using testing::AtMost;
using testing::SaveArg;

namespace {

// A mock class for the AutocompleteController.
class MockAutocompleteController : public AutocompleteController {
 public:
  MockAutocompleteController()
      : AutocompleteController(
            std::make_unique<FakeAutocompleteProviderClient>(),
            AutocompleteControllerConfig{
                .provider_types =
                    AutocompleteClassifier::DefaultOmniboxProviders()}) {}
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

}  // namespace

@interface TestOmniboxAutocompleteController : OmniboxAutocompleteController
@property(nonatomic, assign) NSUInteger lastOpenedSelectionLineIndex;
@property(nonatomic, assign) base::RepeatingClosure openSelectionClosure;
@end

@implementation TestOmniboxAutocompleteController

- (void)openSelection:(OmniboxPopupSelection)selection
            timestamp:(base::TimeTicks)timestamp
          disposition:(WindowOpenDisposition)disposition {
  _lastOpenedSelectionLineIndex = selection.line;
  if (_openSelectionClosure) {
    _openSelectionClosure.Run();
    _openSelectionClosure.Reset();
  }
}
@end

class OmniboxAutocompleteControllerTest : public PlatformTest {
 public:
  OmniboxAutocompleteControllerTest() {
    auto clipboard = std::make_unique<FakeClipboardRecentContent>();
    clipboard_ = clipboard.get();
    ClipboardRecentContent::SetInstance(std::move(clipboard));

    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterLocalStatePrefs(local_state_->registry());
    TestingApplicationContext::GetGlobal()->SetLocalState(local_state_.get());

    omnibox_client_ = std::make_unique<TestOmniboxClient>();

    autocomplete_controller_ = std::make_unique<MockAutocompleteController>();

    omnibox_text_model_ =
        std::make_unique<OmniboxTextModel>(omnibox_client_.get());

    controller_delegate_ =
        OCMProtocolMock(@protocol(OmniboxAutocompleteControllerDelegate));

    controller_ = [[TestOmniboxAutocompleteController alloc]
         initWithOmniboxClient:omnibox_client_.get()
        autocompleteController:autocomplete_controller_.get()
              omniboxTextModel:omnibox_text_model_.get()
           presentationContext:OmniboxPresentationContext::kLocationBar];
    controller_.delegate = controller_delegate_;

    omnibox_metrics_recorder_ = [[OmniboxMetricsRecorder alloc]
        initWithClient:omnibox_client_.get()
             textModel:omnibox_text_model_.get()];
    [omnibox_metrics_recorder_
        setAutocompleteController:controller_.autocompleteController];
    controller_.omniboxMetricsRecorder = omnibox_metrics_recorder_;
  }

  ~OmniboxAutocompleteControllerTest() override {
    [controller_ disconnect];
    clipboard_ = nullptr;
    autocomplete_controller_ = nullptr;
    omnibox_client_ = nullptr;
    omnibox_text_model_ = nullptr;
    controller_delegate_ = nil;
    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
    local_state_.reset();
    [omnibox_metrics_recorder_ disconnect];
    omnibox_metrics_recorder_ = nil;
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
        controller_.lastOpenedSelectionLineIndex);
  }

  /// Simulates opening `url_text` from the text controller.
  void OpenUrlFromEditBox(const std::u16string url_text,
                          bool is_autocompleted) {
    AutocompleteMatch match(autocomplete_controller_->search_provider(), 0,
                            false, AutocompleteMatchType::OPEN_TAB);
    match.destination_url = GURL(url_text);
    match.allowed_to_be_default_match = true;
    if (is_autocompleted) {
      match.inline_autocompletion = url_text;
    } else {
      omnibox_text_model_->SetInputInProgressNoNotify(YES);
      omnibox_text_model_->UpdateUserText(url_text);
    }
    omnibox_text_model_->OnSetFocus();
    [controller_ openMatch:match
                 popupSelection:OmniboxPopupSelection(0)
          windowOpenDisposition:WindowOpenDisposition::CURRENT_TAB
                alternateNavURL:GURL()
                     pastedText:u""
        matchSelectionTimestamp:base::TimeTicks()];
  }

 protected:
  // Message loop for the main test thread.
  base::test::TaskEnvironment environment_;
  // Application pref service.
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  TestOmniboxAutocompleteController* controller_;
  std::unique_ptr<MockAutocompleteController> autocomplete_controller_;
  std::unique_ptr<TestOmniboxClient> omnibox_client_;
  raw_ptr<FakeClipboardRecentContent> clipboard_;
  std::unique_ptr<OmniboxTextModel> omnibox_text_model_;
  OmniboxMetricsRecorder* omnibox_metrics_recorder_;
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
  EXPECT_EQ([controller_ autocompleteController]->result().size(),
            sample_matches.size());
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
                  1, [controller_ autocompleteController]->result().size()));

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
                  1, [controller_ autocompleteController]->result().size()));

  OCMExpect([controller_delegate_ omniboxAutocompleteController:[OCMArg any]
                                     didUpdateSuggestionsGroups:[OCMArg any]]);

  // Request results with more visible than available.
  [controller_ requestSuggestionsWithVisibleSuggestionCount:100];

  EXPECT_OCMOCK_VERIFY(controller_delegate_);
}

// Tests requesting result with part of them visible.
TEST_F(OmniboxAutocompleteControllerTest, RequestResultPartVisible) {
  autocomplete_controller_->SetAutocompleteMatches(SampleMatches());

  size_t result_size = [controller_ autocompleteController]->result().size();
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
  local_state_->SetBoolean(omnibox::kIsOmniboxInBottomPosition, true);
  EXPECT_EQ(autocomplete_controller_->omnibox_position,
            metrics::OmniboxEventProto::BOTTOM_POSITION);

  local_state_->SetBoolean(omnibox::kIsOmniboxInBottomPosition, false);
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
  controller_.lastOpenedSelectionLineIndex =
      OmniboxPopupSelection(UINT_MAX).line;

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
  controller_.openSelectionClosure = open_selection_waiter.QuitClosure();

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

// This verifies the fix for a bug where calling openMatch with a valid
// alternate nav URL would fail a DCHECK if the input began with "http://".
// The failure was due to erroneously trying to strip the scheme from the
// resulting fill_into_edit.  Alternate nav matches are never shown, so there's
// no need to ever try and strip this scheme.
TEST_F(OmniboxAutocompleteControllerTest, AlternateNavHasHTTP) {
  AutocompleteMatch match(autocomplete_controller_->search_provider(), 0, false,
                          AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED);
  // `match.destination_url` has to be set to ensure that OnAutocompleteAccept
  // is called and `alternate_nav_match` is populated.
  match.destination_url = GURL("https://foo/");
  const GURL alternate_nav_url("http://abcd/");

  AutocompleteMatch alternate_nav_match;
  EXPECT_CALL(*omnibox_client_,
              OnAutocompleteAccept(_, _, _, _, _, _, _, _, _, _, _))
      .WillOnce(SaveArg<10>(&alternate_nav_match));

  omnibox_text_model_->OnSetFocus();  // Avoids DCHECK in OpenMatch().
  omnibox_text_model_->SetInputInProgressNoNotify(YES);
  omnibox_text_model_->UpdateUserText(u"http://abcd");
  [controller_ openMatch:match
               popupSelection:OmniboxPopupSelection(0)
        windowOpenDisposition:WindowOpenDisposition::CURRENT_TAB
              alternateNavURL:alternate_nav_url
                   pastedText:u""
      matchSelectionTimestamp:base::TimeTicks()];
  EXPECT_TRUE(
      AutocompleteInput::HasHTTPScheme(alternate_nav_match.fill_into_edit));

  EXPECT_CALL(*omnibox_client_,
              OnAutocompleteAccept(_, _, _, _, _, _, _, _, _, _, _))
      .WillOnce(SaveArg<10>(&alternate_nav_match));

  omnibox_text_model_->SetInputInProgressNoNotify(YES);
  omnibox_text_model_->UpdateUserText(u"abcd");
  [controller_ openMatch:match
               popupSelection:OmniboxPopupSelection(0)
        windowOpenDisposition:WindowOpenDisposition::CURRENT_TAB
              alternateNavURL:alternate_nav_url
                   pastedText:u""
      matchSelectionTimestamp:base::TimeTicks()];

  EXPECT_TRUE(
      AutocompleteInput::HasHTTPScheme(alternate_nav_match.fill_into_edit));
}

#pragma mark - Histogram tests

// Tests IPv4AddressPartsCount logging.
TEST_F(OmniboxAutocompleteControllerTest, IPv4AddressPartsCount) {
  base::HistogramTester histogram_tester;
  constexpr char kIPv4AddressPartsCountHistogramName[] =
      "Omnibox.IPv4AddressPartsCount";
  // Hostnames shall not be recorded.
  OpenUrlFromEditBox(u"http://example.com", false);
  histogram_tester.ExpectTotalCount(kIPv4AddressPartsCountHistogramName, 0);

  // Autocompleted navigations shall not be recorded.
  OpenUrlFromEditBox(u"http://127.0.0.1", true);
  histogram_tester.ExpectTotalCount(kIPv4AddressPartsCountHistogramName, 0);

  // Test IPv4 parts are correctly counted.
  OpenUrlFromEditBox(u"http://127.0.0.1", false);
  OpenUrlFromEditBox(u"http://127.1/test.html", false);
  OpenUrlFromEditBox(u"http://127.0.1", false);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kIPv4AddressPartsCountHistogramName),
      testing::ElementsAre(base::Bucket(2, 1), base::Bucket(3, 1),
                           base::Bucket(4, 1)));
}

// Tests AnswerInSuggest logging.
TEST_F(OmniboxAutocompleteControllerTest, LogAnswerUsed) {
  base::HistogramTester histogram_tester;
  AutocompleteMatch match(autocomplete_controller_->search_provider(), 0, false,
                          AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED);
  match.answer_type = omnibox::ANSWER_TYPE_WEATHER;
  match.destination_url = GURL("https://foo");
  [controller_ openMatch:match
               popupSelection:OmniboxPopupSelection(0)
        windowOpenDisposition:WindowOpenDisposition::CURRENT_TAB
              alternateNavURL:GURL()
                   pastedText:u""
      matchSelectionTimestamp:base::TimeTicks()];
  histogram_tester.ExpectUniqueSample("Omnibox.SuggestionUsed.AnswerInSuggest",
                                      8, 1);
}
