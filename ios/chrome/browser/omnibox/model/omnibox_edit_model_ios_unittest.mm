// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_edit_model_ios.h"

#import <stddef.h>

#import <array>
#import <memory>
#import <string>

#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "build/build_config.h"
#import "components/dom_distiller/core/url_constants.h"
#import "components/dom_distiller/core/url_utils.h"
#import "components/omnibox/browser/actions/omnibox_action.h"
#import "components/omnibox/browser/actions/tab_switch_action.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/omnibox_prefs.h"
#import "components/omnibox/browser/omnibox_triggered_feature_service.h"
#import "components/omnibox/browser/search_provider.h"
#import "components/omnibox/browser/test_location_bar_model.h"
#import "components/omnibox/browser/test_omnibox_client.h"
#import "components/omnibox/browser/test_scheme_classifier.h"
#import "components/omnibox/browser/unscoped_extension_provider.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/prefs/testing_pref_service.h"
#import "components/url_formatter/url_fixer.h"
#import "extensions/buildflags/buildflags.h"
#import "ios/chrome/browser/omnibox/model/omnibox_controller_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_controller.h"
#import "ios/chrome/browser/omnibox/model/test_omnibox_edit_model_ios.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/metrics_proto/omnibox_event.pb.h"
#import "third_party/omnibox_proto/answer_type.pb.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/window_open_disposition.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#import "extensions/common/extension_features.h"  // nogncheck
#endif

using metrics::OmniboxEventProto;
using Selection = OmniboxPopupSelection;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;

namespace ui {
struct AXNodeData;
}

// Mocking the text controller to not rely on the textfield view.
@interface TestOmniboxTextController : OmniboxTextController
@end

@implementation TestOmniboxTextController {
  std::u16string text_;
}

- (std::u16string)displayedText {
  return text_;
}

- (void)setWindowText:(const std::u16string&)text
             caretPos:(size_t)caretPos
    startAutocomplete:(BOOL)startAutocomplete
    notifyTextChanged:(BOOL)notifyTextChanged {
  [super setWindowText:text
               caretPos:caretPos
      startAutocomplete:startAutocomplete
      notifyTextChanged:notifyTextChanged];
  text_ = text;
}

- (void)updateAutocompleteIfTextChanged:(const std::u16string&)userText
                         autocompletion:
                             (const std::u16string&)inlineAutocomplete {
  [super updateAutocompleteIfTextChanged:userText
                          autocompletion:inlineAutocomplete];
  text_ = userText + inlineAutocomplete;
}

@end

namespace {

void OpenUrlFromEditBox(OmniboxControllerIOS* controller,
                        TestOmniboxEditModelIOS* model,
                        OmniboxTextModel* text_model,
                        TestOmniboxTextController* text_controller,
                        const std::u16string url_text,
                        bool is_autocompleted) {
  AutocompleteMatch match(
      controller->autocomplete_controller()->search_provider(), 0, false,
      AutocompleteMatchType::OPEN_TAB);
  match.destination_url = GURL(url_text);
  match.allowed_to_be_default_match = true;
  if (is_autocompleted) {
    match.inline_autocompletion = url_text;
  } else {
    [text_controller setUserText:url_text];
  }
  text_model->OnSetFocus();
  model->OpenMatchForTesting(match, WindowOpenDisposition::CURRENT_TAB, GURL(),
                             std::u16string(), 0);
}

}  // namespace

class OmniboxEditModelIOSTest : public PlatformTest {
 public:
  OmniboxEditModelIOSTest() {
    omnibox_client_ = std::make_unique<TestOmniboxClient>();

    omnibox_controller_ =
        std::make_unique<OmniboxControllerIOS>(omnibox_client_.get());
    omnibox_text_model_ =
        std::make_unique<OmniboxTextModel>(omnibox_client_.get());
    omnibox_metrics_recorder_ = [[OmniboxMetricsRecorder alloc]
        initWithClient:omnibox_client_.get()
             textModel:omnibox_text_model_.get()];
    [omnibox_metrics_recorder_
        setAutocompleteController:omnibox_controller_
                                      ->autocomplete_controller()];

    omnibox_edit_model_ = std::make_unique<TestOmniboxEditModelIOS>(
        omnibox_controller_.get(), omnibox_client_.get(),
        /*pref_service=*/nullptr, omnibox_text_model_.get(),
        omnibox_metrics_recorder_);
    omnibox_text_controller_ = [[TestOmniboxTextController alloc]
        initWithOmniboxController:omnibox_controller_.get()
                    omniboxClient:omnibox_client_.get()
                 omniboxEditModel:omnibox_edit_model_.get()
                 omniboxTextModel:omnibox_text_model_.get()
                    inLensOverlay:NO];
    omnibox_edit_model_->set_text_controller(omnibox_text_controller_);
  }

  ~OmniboxEditModelIOSTest() override {
    [omnibox_metrics_recorder_ disconnect];
    omnibox_metrics_recorder_ = nil;
  }

  TestLocationBarModel* location_bar_model() {
    return omnibox_client_->location_bar_model();
  }
  TestOmniboxEditModelIOS* model() { return omnibox_edit_model_.get(); }
  OmniboxTextModel* text_model() { return omnibox_text_model_.get(); }
  OmniboxControllerIOS* controller() { return omnibox_controller_.get(); }

  bool current_text_is_URL() const {
    return !omnibox_text_model_->user_input_in_progress ||
           !AutocompleteMatch::IsSearchType(
               [omnibox_text_controller_ currentMatch:nullptr].type);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestOmniboxTextController* omnibox_text_controller_;
  OmniboxMetricsRecorder* omnibox_metrics_recorder_;
  std::unique_ptr<TestOmniboxClient> omnibox_client_;
  std::unique_ptr<OmniboxControllerIOS> omnibox_controller_;
  std::unique_ptr<OmniboxTextModel> omnibox_text_model_;
  std::unique_ptr<TestOmniboxEditModelIOS> omnibox_edit_model_;
};

TEST_F(OmniboxEditModelIOSTest, InlineAutocompleteText) {
  // Test if the model updates the inline autocomplete text in the view.
  EXPECT_EQ(std::u16string(), omnibox_text_model_->inline_autocompletion);
  [omnibox_text_controller_ setUserText:u"he"];
  [omnibox_text_controller_ onPopupDataChanged:u"llo"
                                additionalText:std::u16string()
                                      newMatch:{}];
  EXPECT_EQ(u"hello", [omnibox_text_controller_ displayedText]);
  EXPECT_EQ(u"llo", omnibox_text_model_->inline_autocompletion);

  [omnibox_text_controller_ setUserText:u"hel"];
  EXPECT_EQ(std::u16string(), omnibox_text_model_->inline_autocompletion);
  [omnibox_text_controller_ onPopupDataChanged:u"lo"
                                additionalText:std::u16string()
                                      newMatch:{}];
  EXPECT_EQ(u"hello", [omnibox_text_controller_ displayedText]);
  EXPECT_EQ(u"lo", omnibox_text_model_->inline_autocompletion);

  [omnibox_text_controller_ revertState];
  EXPECT_EQ(std::u16string(), [omnibox_text_controller_ displayedText]);
  EXPECT_EQ(std::u16string(), omnibox_text_model_->inline_autocompletion);

  [omnibox_text_controller_ setUserText:u"he"];
  [omnibox_text_controller_ onPopupDataChanged:u"llo"
                                additionalText:std::u16string()
                                      newMatch:{}];
  EXPECT_EQ(u"hello", [omnibox_text_controller_ displayedText]);
  EXPECT_EQ(u"llo", omnibox_text_model_->inline_autocompletion);
}

// This verifies the fix for a bug where calling OpenMatch() with a valid
// alternate nav URL would fail a DCHECK if the input began with "http://".
// The failure was due to erroneously trying to strip the scheme from the
// resulting fill_into_edit.  Alternate nav matches are never shown, so there's
// no need to ever try and strip this scheme.
TEST_F(OmniboxEditModelIOSTest, AlternateNavHasHTTP) {
  AutocompleteMatch match(
      controller()->autocomplete_controller()->search_provider(), 0, false,
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
  [omnibox_text_controller_ setUserText:u"http://abcd"];
  model()->OpenMatchForTesting(match, WindowOpenDisposition::CURRENT_TAB,
                               alternate_nav_url, std::u16string(), 0);
  EXPECT_TRUE(
      AutocompleteInput::HasHTTPScheme(alternate_nav_match.fill_into_edit));

  EXPECT_CALL(*omnibox_client_,
              OnAutocompleteAccept(_, _, _, _, _, _, _, _, _, _, _))
      .WillOnce(SaveArg<10>(&alternate_nav_match));

  [omnibox_text_controller_ setUserText:u"abcd"];
  model()->OpenMatchForTesting(match, WindowOpenDisposition::CURRENT_TAB,
                               alternate_nav_url, std::u16string(), 0);
  EXPECT_TRUE(
      AutocompleteInput::HasHTTPScheme(alternate_nav_match.fill_into_edit));
}

TEST_F(OmniboxEditModelIOSTest, CurrentMatch) {
  // Test the HTTP case.
  {
    location_bar_model()->set_url(GURL("http://www.example.com/"));
    location_bar_model()->set_url_for_display(u"example.com");
    [omnibox_text_controller_ resetDisplayTexts];
    [omnibox_text_controller_ revertState];

    EXPECT_EQ(u"http://www.example.com/",
              [omnibox_text_controller_ displayedText]);

    AutocompleteMatch match = [omnibox_text_controller_ currentMatch:nullptr];
    EXPECT_EQ(AutocompleteMatchType::URL_WHAT_YOU_TYPED, match.type);
    EXPECT_TRUE(current_text_is_URL());
    EXPECT_EQ("http://www.example.com/", match.destination_url.spec());
  }

  // Test that generating a match from an elided HTTPS URL doesn't drop the
  // secure scheme.
  {
    location_bar_model()->set_url(GURL("https://www.google.com/"));
    location_bar_model()->set_url_for_display(u"google.com");
    [omnibox_text_controller_ resetDisplayTexts];
    [omnibox_text_controller_ revertState];

    EXPECT_EQ(u"https://www.google.com/",
              [omnibox_text_controller_ displayedText]);

    AutocompleteMatch match = [omnibox_text_controller_ currentMatch:nullptr];
    EXPECT_EQ(AutocompleteMatchType::URL_WHAT_YOU_TYPED, match.type);
    EXPECT_TRUE(current_text_is_URL());

    // Additionally verify we aren't accidentally dropping the HTTPS scheme.
    EXPECT_EQ("https://www.google.com/", match.destination_url.spec());
  }
}

TEST_F(OmniboxEditModelIOSTest, DisplayText) {
  location_bar_model()->set_url(GURL("https://www.example.com/"));
  location_bar_model()->set_url_for_display(u"example.com");

  EXPECT_TRUE([omnibox_text_controller_ resetDisplayTexts]);
  [omnibox_text_controller_ revertState];

  EXPECT_TRUE(current_text_is_URL());

  // iOS OmniboxEditModel always provides the full URL as the OmniboxView
  // permanent display text.
  EXPECT_EQ(u"https://www.example.com/", omnibox_text_model_->url_for_editing);
  EXPECT_EQ(u"https://www.example.com/",
            [omnibox_text_controller_ displayedText]);
  EXPECT_FALSE(omnibox_text_model_->user_input_in_progress);

  EXPECT_EQ(u"https://www.example.com/",
            [omnibox_text_controller_ displayedText]);
  EXPECT_TRUE(current_text_is_URL());
}

TEST_F(OmniboxEditModelIOSTest, IPv4AddressPartsCount) {
  base::HistogramTester histogram_tester;
  constexpr char kIPv4AddressPartsCountHistogramName[] =
      "Omnibox.IPv4AddressPartsCount";
  // Hostnames shall not be recorded.
  OpenUrlFromEditBox(controller(), model(), text_model(),
                     omnibox_text_controller_, u"http://example.com", false);
  histogram_tester.ExpectTotalCount(kIPv4AddressPartsCountHistogramName, 0);

  // Autocompleted navigations shall not be recorded.
  OpenUrlFromEditBox(controller(), model(), text_model(),
                     omnibox_text_controller_, u"http://127.0.0.1", true);
  histogram_tester.ExpectTotalCount(kIPv4AddressPartsCountHistogramName, 0);

  // Test IPv4 parts are correctly counted.
  OpenUrlFromEditBox(controller(), model(), text_model(),
                     omnibox_text_controller_, u"http://127.0.0.1", false);
  OpenUrlFromEditBox(controller(), model(), text_model(),
                     omnibox_text_controller_, u"http://127.1/test.html",
                     false);
  OpenUrlFromEditBox(controller(), model(), text_model(),
                     omnibox_text_controller_, u"http://127.0.1", false);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kIPv4AddressPartsCountHistogramName),
      testing::ElementsAre(base::Bucket(2, 1), base::Bucket(3, 1),
                           base::Bucket(4, 1)));
}

TEST_F(OmniboxEditModelIOSTest, LogAnswerUsed) {
  base::HistogramTester histogram_tester;
  AutocompleteMatch match(
      controller()->autocomplete_controller()->search_provider(), 0, false,
      AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED);
  match.answer_type = omnibox::ANSWER_TYPE_WEATHER;
  match.destination_url = GURL("https://foo");
  model()->OpenMatchForTesting(match, WindowOpenDisposition::CURRENT_TAB,
                               GURL(), std::u16string(), 0);
  histogram_tester.ExpectUniqueSample("Omnibox.SuggestionUsed.AnswerInSuggest",
                                      8, 1);
}

