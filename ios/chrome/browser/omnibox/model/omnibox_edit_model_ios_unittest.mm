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
#import "ios/chrome/browser/omnibox/model/omnibox_view_ios.h"
#import "ios/chrome/browser/omnibox/model/test_omnibox_edit_model_ios.h"
#import "ios/chrome/browser/omnibox/model/test_omnibox_popup_view_ios.h"
#import "ios/chrome/browser/omnibox/model/test_omnibox_view_ios.h"
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

namespace {

void OpenUrlFromEditBox(OmniboxControllerIOS* controller,
                        TestOmniboxEditModelIOS* model,
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
    model->SetUserText(url_text);
  }
  model->OnSetFocus();
  model->OpenMatchForTesting(match, WindowOpenDisposition::CURRENT_TAB, GURL(),
                             std::u16string(), 0);
}

}  // namespace

class OmniboxEditModelIOSTest : public PlatformTest {
 public:
  OmniboxEditModelIOSTest() {
    auto omnibox_client = std::make_unique<TestOmniboxClient>();
    omnibox_client_ = omnibox_client.get();

    view_ = std::make_unique<TestOmniboxViewIOS>(std::move(omnibox_client));
    view_->controller()->SetEditModelForTesting(
        std::make_unique<TestOmniboxEditModelIOS>(view_->controller(),
                                                  view_.get(),
                                                  /*pref_service=*/nullptr));
  }

  TestOmniboxViewIOS* view() { return view_.get(); }
  TestLocationBarModel* location_bar_model() {
    return omnibox_client_->location_bar_model();
  }
  TestOmniboxEditModelIOS* model() {
    return static_cast<TestOmniboxEditModelIOS*>(view_->model());
  }
  OmniboxControllerIOS* controller() { return view_->controller(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  raw_ptr<TestOmniboxClient, DanglingUntriaged> omnibox_client_;
  std::unique_ptr<TestOmniboxViewIOS> view_;
};

TEST_F(OmniboxEditModelIOSTest, DISABLED_InlineAutocompleteText) {
  // Test if the model updates the inline autocomplete text in the view.
  EXPECT_EQ(std::u16string(), view()->inline_autocompletion());
  model()->SetUserText(u"he");
  model()->OnPopupDataChanged(u"llo", std::u16string(), {});
  EXPECT_EQ(u"hello", view()->GetText());
  EXPECT_EQ(u"llo", view()->inline_autocompletion());

  std::u16string text_before = u"he";
  std::u16string text_after = u"hel";
  OmniboxViewIOS::StateChanges state_changes{&text_before, &text_after, 3,    3,
                                             false,        true,        false};
  model()->OnAfterPossibleChange(state_changes);
  EXPECT_EQ(std::u16string(), view()->inline_autocompletion());
  model()->OnPopupDataChanged(u"lo", std::u16string(), {});
  EXPECT_EQ(u"hello", view()->GetText());
  EXPECT_EQ(u"lo", view()->inline_autocompletion());

  model()->Revert();
  EXPECT_EQ(std::u16string(), view()->GetText());
  EXPECT_EQ(std::u16string(), view()->inline_autocompletion());

  model()->SetUserText(u"he");
  model()->OnPopupDataChanged(u"llo", std::u16string(), {});
  EXPECT_EQ(u"hello", view()->GetText());
  EXPECT_EQ(u"llo", view()->inline_autocompletion());
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

  model()->OnSetFocus();  // Avoids DCHECK in OpenMatch().
  model()->SetUserText(u"http://abcd");
  model()->OpenMatchForTesting(match, WindowOpenDisposition::CURRENT_TAB,
                               alternate_nav_url, std::u16string(), 0);
  EXPECT_TRUE(
      AutocompleteInput::HasHTTPScheme(alternate_nav_match.fill_into_edit));

  EXPECT_CALL(*omnibox_client_,
              OnAutocompleteAccept(_, _, _, _, _, _, _, _, _, _, _))
      .WillOnce(SaveArg<10>(&alternate_nav_match));

  model()->SetUserText(u"abcd");
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
    model()->ResetDisplayTexts();
    model()->Revert();

    EXPECT_EQ(u"http://www.example.com/", view()->GetText());

    AutocompleteMatch match = model()->CurrentMatch(nullptr);
    EXPECT_EQ(AutocompleteMatchType::URL_WHAT_YOU_TYPED, match.type);
    EXPECT_TRUE(model()->CurrentTextIsURL());
    EXPECT_EQ("http://www.example.com/", match.destination_url.spec());
  }

  // Test that generating a match from an elided HTTPS URL doesn't drop the
  // secure scheme.
  {
    location_bar_model()->set_url(GURL("https://www.google.com/"));
    location_bar_model()->set_url_for_display(u"google.com");
    model()->ResetDisplayTexts();
    model()->Revert();

    EXPECT_EQ(u"https://www.google.com/", view()->GetText());

    AutocompleteMatch match = model()->CurrentMatch(nullptr);
    EXPECT_EQ(AutocompleteMatchType::URL_WHAT_YOU_TYPED, match.type);
    EXPECT_TRUE(model()->CurrentTextIsURL());

    // Additionally verify we aren't accidentally dropping the HTTPS scheme.
    EXPECT_EQ("https://www.google.com/", match.destination_url.spec());
  }
}

TEST_F(OmniboxEditModelIOSTest, DisplayText) {
  location_bar_model()->set_url(GURL("https://www.example.com/"));
  location_bar_model()->set_url_for_display(u"example.com");

  EXPECT_TRUE(model()->ResetDisplayTexts());
  model()->Revert();

  EXPECT_TRUE(model()->CurrentTextIsURL());

  // iOS OmniboxEditModel always provides the full URL as the OmniboxView
  // permanent display text.
  EXPECT_EQ(u"https://www.example.com/", model()->GetPermanentDisplayText());
  EXPECT_EQ(u"https://www.example.com/", view()->GetText());
  EXPECT_FALSE(model()->user_input_in_progress());

  EXPECT_EQ(u"https://www.example.com/", view()->GetText());
  EXPECT_TRUE(model()->CurrentTextIsURL());
}

///////////////////////////////////////////////////////////////////////////////
// Popup-related tests

class OmniboxEditModelIOSPopupTest : public PlatformTest {
 public:
  OmniboxEditModelIOSPopupTest() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    // `kExperimentalOmniboxLabs` feature flag has to be enabled
    // before the test client initialization for the `UnscopedExtensionProvider`
    // to be initialized. The provider is needed for
    // `GetIconForExtensionWithImageURL` test.
    feature_list_.InitAndEnableFeature(
        extensions_features::kExperimentalOmniboxLabs);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

    auto omnibox_client = std::make_unique<TestOmniboxClient>();
    EXPECT_CALL(*omnibox_client, GetPrefs())
        .WillRepeatedly(Return(pref_service()));

    view_ = std::make_unique<TestOmniboxViewIOS>(std::move(omnibox_client));
    view_->controller()->SetEditModelForTesting(
        std::make_unique<TestOmniboxEditModelIOS>(view_->controller(),
                                                  view_.get(), pref_service()));

    omnibox::RegisterProfilePrefs(pref_service_.registry());
    model()->set_popup_view(&popup_view_);
    model()->SetPopupIsOpen(true);
  }
  OmniboxEditModelIOSPopupTest(const OmniboxEditModelIOSPopupTest&) = delete;
  OmniboxEditModelIOSPopupTest& operator=(const OmniboxEditModelIOSPopupTest&) =
      delete;

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }
  OmniboxTriggeredFeatureService* triggered_feature_service() {
    return &triggered_feature_service_;
  }
  TestOmniboxEditModelIOS* model() {
    return static_cast<TestOmniboxEditModelIOS*>(view_->model());
  }
  OmniboxControllerIOS* controller() { return view_->controller(); }
  TestOmniboxClient* client() {
    return static_cast<TestOmniboxClient*>(controller()->client());
  }
  AutocompleteResult* published_result() {
    return const_cast<AutocompleteResult*>(
        &view_->controller()->autocomplete_controller()->result());
  }
  AutocompleteInput& autocomplete_input() {
    return const_cast<AutocompleteInput&>(
        view_->controller()->autocomplete_controller()->input());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TestOmniboxViewIOS> view_;
  TestOmniboxPopupViewIOS popup_view_;
  OmniboxTriggeredFeatureService triggered_feature_service_;
};

TEST_F(OmniboxEditModelIOSTest, IPv4AddressPartsCount) {
  base::HistogramTester histogram_tester;
  constexpr char kIPv4AddressPartsCountHistogramName[] =
      "Omnibox.IPv4AddressPartsCount";
  // Hostnames shall not be recorded.
  OpenUrlFromEditBox(controller(), model(), u"http://example.com", false);
  histogram_tester.ExpectTotalCount(kIPv4AddressPartsCountHistogramName, 0);

  // Autocompleted navigations shall not be recorded.
  OpenUrlFromEditBox(controller(), model(), u"http://127.0.0.1", true);
  histogram_tester.ExpectTotalCount(kIPv4AddressPartsCountHistogramName, 0);

  // Test IPv4 parts are correctly counted.
  OpenUrlFromEditBox(controller(), model(), u"http://127.0.0.1", false);
  OpenUrlFromEditBox(controller(), model(), u"http://127.1/test.html", false);
  OpenUrlFromEditBox(controller(), model(), u"http://127.0.1", false);
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

