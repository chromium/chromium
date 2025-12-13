// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_text_controller.h"

#import "base/test/task_environment.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_provider_client.h"
#import "components/omnibox/browser/omnibox_prefs.h"
#import "components/omnibox/browser/test_location_bar_model.h"
#import "components/omnibox/browser/test_omnibox_client.h"
#import "components/prefs/testing_pref_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_model.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/testing_application_context.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

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

class OmniboxTextControllerTest : public PlatformTest {
 public:
  OmniboxTextControllerTest() {
    omnibox_client_ = std::make_unique<TestOmniboxClient>();
    omnibox::RegisterProfilePrefs(
        static_cast<sync_preferences::TestingPrefServiceSyncable*>(
            classifier_pref_service())
            ->registry());

    omnibox_text_model_ =
        std::make_unique<OmniboxTextModel>(omnibox_client_.get());

    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterLocalStatePrefs(local_state_->registry());
    TestingApplicationContext::GetGlobal()->SetLocalState(local_state_.get());

    autocomplete_controller_ = std::make_unique<AutocompleteController>(
        omnibox_client_->CreateAutocompleteProviderClient(),
        AutocompleteControllerConfig{
            .provider_types =
                AutocompleteClassifier::DefaultOmniboxProviders()});

    omnibox_autocomplete_controller_ = [[OmniboxAutocompleteController alloc]
         initWithOmniboxClient:omnibox_client_.get()
        autocompleteController:autocomplete_controller_.get()
              omniboxTextModel:omnibox_text_model_.get()
           presentationContext:OmniboxPresentationContext::kLocationBar];

    omnibox_text_controller_ = [[TestOmniboxTextController alloc]
        initWithOmniboxClient:omnibox_client_.get()
             omniboxTextModel:omnibox_text_model_.get()
          presentationContext:OmniboxPresentationContext::kLocationBar];

    omnibox_text_controller_.omniboxAutocompleteController =
        omnibox_autocomplete_controller_;
  }

  ~OmniboxTextControllerTest() override {
    [omnibox_text_controller_ disconnect];
    omnibox_text_controller_ = nil;
    [omnibox_autocomplete_controller_ disconnect];
    omnibox_autocomplete_controller_ = nil;
    autocomplete_controller_.reset();
    omnibox_client_.reset();
    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
    local_state_.reset();
  }

  PrefService* classifier_pref_service() {
    return omnibox_client_->autocomplete_classifier()
        ->autocomplete_controller()
        ->autocomplete_provider_client()
        ->GetPrefs();
  }

  TestLocationBarModel* location_bar_model() {
    return omnibox_client_->location_bar_model();
  }

  bool current_text_is_URL() const {
    return !omnibox_text_model_->user_input_in_progress ||
           !AutocompleteMatch::IsSearchType(
               [omnibox_text_controller_ currentMatch:nullptr].type);
  }

 protected:
  base::test::TaskEnvironment environment_;

  std::unique_ptr<AutocompleteController> autocomplete_controller_;
  OmniboxAutocompleteController* omnibox_autocomplete_controller_;
  TestOmniboxTextController* omnibox_text_controller_;
  std::unique_ptr<OmniboxTextModel> omnibox_text_model_;
  std::unique_ptr<TestOmniboxClient> omnibox_client_;
  // Application pref service.
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
};

// Tests if the controller updates the inline autocomplete text.
TEST_F(OmniboxTextControllerTest, InlineAutocompleteText) {
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

TEST_F(OmniboxTextControllerTest, CurrentMatch) {
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

TEST_F(OmniboxTextControllerTest, DisplayText) {
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

// Tests that calling onTextChanged after disconnect doesn't crash.
TEST_F(OmniboxTextControllerTest, OnTextChangedAfterDisconnect) {
  [omnibox_text_controller_ disconnect];
  [omnibox_text_controller_ onTextChanged];
  // The test passes if it doesn't crash.
}

// Tests that calling getInfoForCurrentText after disconnect doesn't crash.
TEST_F(OmniboxTextControllerTest, GetInfoForCurrentTextAfterDisconnect) {
  [omnibox_text_controller_ disconnect];
  AutocompleteMatch match;
  GURL alternate_url;
  [omnibox_text_controller_ getInfoForCurrentText:&match
                           alternateNavigationURL:&alternate_url];
  // The test passes if it doesn't crash.
}

// Tests that calling setUserText after disconnect doesn't crash.
TEST_F(OmniboxTextControllerTest, SetUserTextAfterDisconnect) {
  [omnibox_text_controller_ disconnect];
  [omnibox_text_controller_ setUserText:u"test"];
  // The test passes if it doesn't crash.
}

// Tests that calling currentMatch after disconnect doesn't crash.
TEST_F(OmniboxTextControllerTest, CurrentMatchAfterDisconnect) {
  [omnibox_text_controller_ disconnect];
  GURL alternate_url;
  [omnibox_text_controller_ currentMatch:&alternate_url];
  // The test passes if it doesn't crash.
}

// Tests that calling onPopupDataChanged after disconnect doesn't crash.
TEST_F(OmniboxTextControllerTest, OnPopupDataChangedAfterDisconnect) {
  [omnibox_text_controller_ disconnect];
  [omnibox_text_controller_ onPopupDataChanged:u"inline"
                                additionalText:u"additional"
                                      newMatch:AutocompleteMatch()];
  // The test passes if it doesn't crash.
}

// Tests that calling onCopy after disconnect doesn't crash.
TEST_F(OmniboxTextControllerTest, OnCopyAfterDisconnect) {
  [omnibox_text_controller_ disconnect];
  [omnibox_text_controller_ onCopy];
  // The test passes if it doesn't crash.
}
