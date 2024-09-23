// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "components/language/core/browser/language_prefs.h"
#import "components/language/core/browser/pref_names.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper.h"
#import "components/language_detection/core/language_detection_model.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/translate/core/browser/mock_translate_ranker.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "components/translate/core/language_detection/language_detection_model.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_client.h"
#import "ios/web_view/internal/translate/cwv_translation_controller_internal.h"
#import "ios/web_view/internal/translate/cwv_translation_language_internal.h"
#import "ios/web_view/internal/translate/web_view_translate_client.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_translation_controller_delegate.h"
#import "ios/web_view/public/cwv_translation_policy.h"
#import "ios/web_view/test/test_with_locale_and_resources.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace ios_web_view {

using testing::_;
using testing::Invoke;
using testing::Return;

namespace {
NSString* const kTestFromLangCode = @"ja";
NSString* const kTestToLangCode = @"en";
NSString* const kTestPageHost = @"www.chromium.org";
}  // namespace

class MockWebViewTranslateClient : public WebViewTranslateClient {
 public:
  MockWebViewTranslateClient(
      PrefService* pref_service,
      translate::TranslateRanker* translate_ranker,
      language::LanguageModel* language_model,
      language::UrlLanguageHistogram* url_language_histogram,
      web::WebState* web_state,
      language::AcceptLanguagesService* accept_languages)
      : WebViewTranslateClient(pref_service,
                               translate_ranker,
                               language_model,
                               url_language_histogram,
                               web_state,
                               accept_languages) {}
  MOCK_METHOD3(TranslatePage,
               void(const std::string&, const std::string&, bool));
  MOCK_METHOD0(RevertTranslation, void());
  MOCK_METHOD0(RequestTranslationOffer, bool());
};

class TestLanguageModel : public language::LanguageModel {
  std::vector<LanguageDetails> GetLanguages() override { return {}; }
};

class CWVTranslationControllerTest : public TestWithLocaleAndResources {
 protected:
  CWVTranslationControllerTest()
      : language_detection_model_(
            std::make_unique<language_detection::LanguageDetectionModel>()) {
    web::WebState::CreateParams params(&browser_state_);
    web_state_ = web::WebState::Create(params);
    web_state_->SetKeepRenderProcessAlive(true);

    pref_service_.registry()->RegisterStringPref(
        language::prefs::kAcceptLanguages, "en");
    pref_service_.registry()->RegisterStringPref(
        language::prefs::kSelectedLanguages, "");
    pref_service_.registry()->RegisterListPref(
        language::prefs::kForcedLanguages);
    pref_service_.registry()->RegisterListPref(
        translate::prefs::kBlockedLanguages,
        translate::TranslatePrefs::GetDefaultBlockedLanguages());
    pref_service_.registry()->RegisterBooleanPref(
        translate::prefs::kOfferTranslateEnabled, true);
    pref_service_.registry()->RegisterListPref(
        translate::TranslatePrefs::kPrefNeverPromptSitesDeprecated);
    pref_service_.registry()->RegisterDictionaryPref(
        translate::prefs::kPrefNeverPromptSitesWithTime);
    pref_service_.registry()->RegisterDictionaryPref(
        translate::prefs::kPrefAlwaysTranslateList);
    pref_service_.registry()->RegisterDictionaryPref(
        translate::TranslatePrefs::kPrefAlwaysTranslateListDeprecated);
    pref_service_.registry()->RegisterDictionaryPref(
        translate::TranslatePrefs::kPrefTranslateDeniedCount);
    pref_service_.registry()->RegisterDictionaryPref(
        translate::TranslatePrefs::kPrefTranslateIgnoredCount);
    pref_service_.registry()->RegisterDictionaryPref(
        translate::TranslatePrefs::kPrefTranslateAcceptedCount);
    pref_service_.registry()->RegisterStringPref(
        translate::prefs::kPrefTranslateRecentTarget, "");
    // Using string literal here because kForceTriggerTranslateCount is private
    // in translate::TranslatePrefs.
    pref_service_.registry()->RegisterIntegerPref(
        "translate_force_trigger_on_english_count_for_backoff_1", 0);
    pref_service_.registry()->RegisterDictionaryPref(
        translate::TranslatePrefs::kPrefTranslateAutoAlwaysCount);
    pref_service_.registry()->RegisterDictionaryPref(
        translate::TranslatePrefs::kPrefTranslateAutoNeverCount);

    accept_languages_ = std::make_unique<language::AcceptLanguagesService>(
        &pref_service_, language::prefs::kAcceptLanguages);

    language::IOSLanguageDetectionTabHelper::CreateForWebState(
        web_state_.get(),
        /*url_language_histogram=*/nullptr, &language_detection_model_,
        &pref_service_);

    auto translate_client = std::make_unique<MockWebViewTranslateClient>(
        &pref_service_, &translate_ranker_, &language_model_,
        /*url_language_histogram=*/nullptr, web_state_.get(),
        accept_languages_.get());
    translate_client_ = translate_client.get();

    translation_controller_ = [[CWVTranslationController alloc]
        initWithWebState:web_state_.get()
         translateClient:std::move(translate_client)];

    translate_prefs_ = translate_client_->GetTranslatePrefs();
    translate_prefs_->ResetToDefaults();
  }

  ~CWVTranslationControllerTest() override {
    translate_prefs_->ResetToDefaults();
  }

  // Checks if |lang_code| matches the OCMArg's CWVTranslationLanguage.
  [[nodiscard]] id CheckLanguageCode(NSString* lang_code) {
    return [OCMArg checkWithBlock:^BOOL(CWVTranslationLanguage* lang) {
      return [lang.languageCode isEqualToString:lang_code];
    }];
  }

  web::WebTaskEnvironment task_environment_;
  translate::testing::MockTranslateRanker translate_ranker_;
  TestLanguageModel language_model_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<language::AcceptLanguagesService> accept_languages_;
  web::FakeBrowserState browser_state_;
  std::unique_ptr<web::WebState> web_state_;
  MockWebViewTranslateClient* translate_client_;
  CWVTranslationController* translation_controller_;
  std::unique_ptr<translate::TranslatePrefs> translate_prefs_;
  translate::LanguageDetectionModel language_detection_model_;
};

// Tests CWVTranslationController invokes can offer delegate method.
TEST_F(CWVTranslationControllerTest, CanOfferCallback) {
  id delegate = OCMProtocolMock(@protocol(CWVTranslationControllerDelegate));
  translation_controller_.delegate = delegate;

  OCMExpect([delegate
                translationController:translation_controller_
      canOfferTranslationFromLanguage:CheckLanguageCode(kTestFromLangCode)
                           toLanguage:CheckLanguageCode(kTestToLangCode)]);
  translate_client_->ShowTranslateUI(translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
                                     base::SysNSStringToUTF8(kTestFromLangCode),
                                     base::SysNSStringToUTF8(kTestToLangCode),
                                     translate::TranslateErrors::NONE,
                                     /*triggered_from_menu=*/false);

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests CWVTranslationController invokes did start delegate method.
TEST_F(CWVTranslationControllerTest, DidStartCallback) {
  id delegate = OCMProtocolMock(@protocol(CWVTranslationControllerDelegate));
  translation_controller_.delegate = delegate;

  OCMExpect([delegate translationController:translation_controller_
            didStartTranslationFromLanguage:CheckLanguageCode(kTestFromLangCode)
                                 toLanguage:CheckLanguageCode(kTestToLangCode)
                              userInitiated:YES]);
  translate_client_->ShowTranslateUI(translate::TRANSLATE_STEP_TRANSLATING,
                                     base::SysNSStringToUTF8(kTestFromLangCode),
                                     base::SysNSStringToUTF8(kTestToLangCode),
                                     translate::TranslateErrors::NONE,
                                     /*triggered_from_menu=*/true);

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests CWVTranslationController invokes did finish delegate method.
TEST_F(CWVTranslationControllerTest, DidFinishCallback) {
  id delegate = OCMProtocolMock(@protocol(CWVTranslationControllerDelegate));
  translation_controller_.delegate = delegate;

  id check_error_code = [OCMArg checkWithBlock:^BOOL(NSError* error) {
    return error.code == CWVTranslationErrorInitializationError;
  }];
  OCMExpect([delegate translationController:translation_controller_
           didFinishTranslationFromLanguage:CheckLanguageCode(kTestFromLangCode)
                                 toLanguage:CheckLanguageCode(kTestToLangCode)
                                      error:check_error_code]);
  translate_client_->ShowTranslateUI(
      translate::TRANSLATE_STEP_AFTER_TRANSLATE,
      base::SysNSStringToUTF8(kTestFromLangCode),
      base::SysNSStringToUTF8(kTestToLangCode),
      translate::TranslateErrors::INITIALIZATION_ERROR,
      /*triggered_from_menu=*/false);

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests CWVTranslationController has at least one supported language.
TEST_F(CWVTranslationControllerTest, HasSupportedLanguages) {
  EXPECT_LT(0ul, translation_controller_.supportedLanguages.count);
}

// Tests CWVTranslationController properly sets language policies.
TEST_F(CWVTranslationControllerTest, SetLanguagePolicy) {
  CWVTranslationLanguage* lang =
      [translation_controller_.supportedLanguages anyObject];
  std::string lang_code = base::SysNSStringToUTF8(lang.languageCode);
  CWVTranslationPolicy* policy = [CWVTranslationPolicy translationPolicyNever];
  [translation_controller_ setTranslationPolicy:policy forPageLanguage:lang];
  EXPECT_TRUE(translate_prefs_->IsBlockedLanguage(lang_code));
}

// Tests CWVTranslationController properly reads language policies.
TEST_F(CWVTranslationControllerTest, ReadLanguagePolicy) {
  CWVTranslationLanguage* lang =
      [translation_controller_.supportedLanguages anyObject];
  std::string lang_code = base::SysNSStringToUTF8(lang.languageCode);
  translate_prefs_->AddToLanguageList(lang_code, /*force_blocked=*/true);
  CWVTranslationPolicy* policy =
      [translation_controller_ translationPolicyForPageLanguage:lang];
  EXPECT_EQ(CWVTranslationPolicyNever, policy.type);
  EXPECT_NSEQ(nil, policy.language);
}

// Tests CWVTranslationController properly sets page host policies.
TEST_F(CWVTranslationControllerTest, PageHostPolicy) {
  CWVTranslationPolicy* policy = [CWVTranslationPolicy translationPolicyNever];
  [translation_controller_ setTranslationPolicy:policy
                                    forPageHost:kTestPageHost];
  EXPECT_TRUE(translate_prefs_->IsSiteOnNeverPromptList(
      base::SysNSStringToUTF8(kTestPageHost)));
}

// Tests CWVTranslationController properly reads page host policies.
TEST_F(CWVTranslationControllerTest, ReadPageHostPolicy) {
  translate_prefs_->AddSiteToNeverPromptList(
      base::SysNSStringToUTF8(kTestPageHost));
  CWVTranslationPolicy* policy =
      [translation_controller_ translationPolicyForPageHost:kTestPageHost];
  EXPECT_EQ(CWVTranslationPolicyNever, policy.type);
  EXPECT_NSEQ(nil, policy.language);
}

// Tests CWVTranslationController translate page method.
TEST_F(CWVTranslationControllerTest, TranslatePage) {
  NSArray* langs = translation_controller_.supportedLanguages.allObjects;
  CWVTranslationLanguage* from_lang = langs.firstObject;
  CWVTranslationLanguage* to_lang = langs.lastObject;
  std::string from_code = base::SysNSStringToUTF8(from_lang.languageCode);
  std::string to_code = base::SysNSStringToUTF8(to_lang.languageCode);

  EXPECT_CALL(*translate_client_, TranslatePage(from_code, to_code, true));

  [translation_controller_ translatePageFromLanguage:from_lang
                                          toLanguage:to_lang
                                       userInitiated:YES];
}

// Tests CWVTranslationController revert method.
TEST_F(CWVTranslationControllerTest, Revert) {
  EXPECT_CALL(*translate_client_, RevertTranslation);
  [translation_controller_ revertTranslation];
}

// Tests CWVTranslationController request translation offer method.
TEST_F(CWVTranslationControllerTest, RequestTranslationOffer) {
  EXPECT_CALL(*translate_client_, RequestTranslationOffer)
      .WillOnce(Return(true));
  EXPECT_TRUE([translation_controller_ requestTranslationOffer]);
}

}  // namespace ios_web_view
