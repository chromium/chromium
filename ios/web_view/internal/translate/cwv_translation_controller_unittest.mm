// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/translate/cwv_translation_controller_internal.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/strings/sys_string_conversions.h"
#include "components/language/ios/browser/ios_language_detection_tab_helper.h"
#include "components/translate/core/browser/translate_prefs.h"
#import "ios/web/public/deprecated/crw_test_js_injection_receiver.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/web_client.h"
#import "ios/web_view/internal/translate/cwv_translation_language_internal.h"
#import "ios/web_view/internal/translate/fake_web_view_translate_client.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_translation_controller_delegate.h"
#import "ios/web_view/public/cwv_translation_policy.h"
#include "ios/web_view/test/test_with_locale_and_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

namespace {
NSString* const kTestFromLangCode = @"ja";
NSString* const kTestToLangCode = @"en";
NSString* const kTestPageHost = @"www.chromium.org";
}  // namespace

class CWVTranslationControllerTest : public TestWithLocaleAndResources {
 protected:
  CWVTranslationControllerTest()
      : web_client_(std::make_unique<web::WebClient>()),
        browser_state_(/*off_the_record=*/false) {
    web_state_.SetBrowserState(&browser_state_);
    auto test_navigation_manager =
        std::make_unique<web::TestNavigationManager>();
    web_state_.SetNavigationManager(std::move(test_navigation_manager));
    CRWTestJSInjectionReceiver* injection_receiver =
        [[CRWTestJSInjectionReceiver alloc] init];
    web_state_.SetJSInjectionReceiver(injection_receiver);
    language::IOSLanguageDetectionTabHelper::CreateForWebState(
        &web_state_,
        /*url_language_histogram=*/nullptr);
    translate_client_ = std::make_unique<FakeWebViewTranslateClient>(
        &web_state_, /*page_lang=*/"en");
    translation_controller_ = [[CWVTranslationController alloc]
        initWithTranslateClient:translate_client_.get()];
    translate_prefs_ = translate_client_->GetTranslatePrefs();
    translate_prefs_->ResetToDefaults();
  }

  ~CWVTranslationControllerTest() override {
    translate_prefs_->ResetToDefaults();
  }

  // Checks if |lang_code| matches the OCMArg's CWVTranslationLanguage.
  id CheckLanguageCode(NSString* lang_code) WARN_UNUSED_RESULT {
    return [OCMArg checkWithBlock:^BOOL(CWVTranslationLanguage* lang) {
      return [lang.languageCode isEqualToString:lang_code];
    }];
  }

  web::WebTaskEnvironment task_environment_;
  web::ScopedTestingWebClient web_client_;
  WebViewBrowserState browser_state_;
  std::unique_ptr<FakeWebViewTranslateClient> translate_client_;
  web::TestWebState web_state_;
  CWVTranslationController* translation_controller_;
  std::unique_ptr<translate::TranslatePrefs> translate_prefs_;
};

// Tests CWVTranslationController invokes can offer delegate method.
TEST_F(CWVTranslationControllerTest, CanOfferCallback) {
  id delegate = OCMProtocolMock(@protocol(CWVTranslationControllerDelegate));
  translation_controller_.delegate = delegate;

  [[delegate expect] translationController:translation_controller_
           canOfferTranslationFromLanguage:CheckLanguageCode(kTestFromLangCode)
                                toLanguage:CheckLanguageCode(kTestToLangCode)];
  translate_client_->ShowTranslateUI(translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
                                     base::SysNSStringToUTF8(kTestFromLangCode),
                                     base::SysNSStringToUTF8(kTestToLangCode),
                                     translate::TranslateErrors::NONE,
                                     /*triggered_from_menu=*/false);

  [delegate verify];
}

// Tests CWVTranslationController invokes did start delegate method.
TEST_F(CWVTranslationControllerTest, DidStartCallback) {
  id delegate = OCMProtocolMock(@protocol(CWVTranslationControllerDelegate));
  translation_controller_.delegate = delegate;

  [[delegate expect] translationController:translation_controller_
           didStartTranslationFromLanguage:CheckLanguageCode(kTestFromLangCode)
                                toLanguage:CheckLanguageCode(kTestToLangCode)
                             userInitiated:YES];
  translate_client_->ShowTranslateUI(translate::TRANSLATE_STEP_TRANSLATING,
                                     base::SysNSStringToUTF8(kTestFromLangCode),
                                     base::SysNSStringToUTF8(kTestToLangCode),
                                     translate::TranslateErrors::NONE,
                                     /*triggered_from_menu=*/true);

  [delegate verify];
}

// Tests CWVTranslationController invokes did finish delegate method.
TEST_F(CWVTranslationControllerTest, DidFinishCallback) {
  id delegate = OCMProtocolMock(@protocol(CWVTranslationControllerDelegate));
  translation_controller_.delegate = delegate;

  id check_error_code = [OCMArg checkWithBlock:^BOOL(NSError* error) {
    return error.code == CWVTranslationErrorInitializationError;
  }];
  [[delegate expect] translationController:translation_controller_
          didFinishTranslationFromLanguage:CheckLanguageCode(kTestFromLangCode)
                                toLanguage:CheckLanguageCode(kTestToLangCode)
                                     error:check_error_code];
  translate_client_->ShowTranslateUI(
      translate::TRANSLATE_STEP_AFTER_TRANSLATE,
      base::SysNSStringToUTF8(kTestFromLangCode),
      base::SysNSStringToUTF8(kTestToLangCode),
      translate::TranslateErrors::INITIALIZATION_ERROR,
      /*triggered_from_menu=*/false);

  [delegate verify];
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
  EXPECT_TRUE(translate_prefs_->IsSiteBlacklisted(
      base::SysNSStringToUTF8(kTestPageHost)));
}

// Tests CWVTranslationController properly reads page host policies.
TEST_F(CWVTranslationControllerTest, ReadPageHostPolicy) {
  translate_prefs_->BlacklistSite(base::SysNSStringToUTF8(kTestPageHost));
  CWVTranslationPolicy* policy =
      [translation_controller_ translationPolicyForPageHost:kTestPageHost];
  EXPECT_EQ(CWVTranslationPolicyNever, policy.type);
  EXPECT_NSEQ(nil, policy.language);
}

// Tests CWVTranslationController translate page and revert methods.
TEST_F(CWVTranslationControllerTest, TranslatePageAndRevert) {
  NSArray* langs = translation_controller_.supportedLanguages.allObjects;
  CWVTranslationLanguage* from_lang = langs.firstObject;
  CWVTranslationLanguage* to_lang = langs.lastObject;
  std::string from_code = base::SysNSStringToUTF8(from_lang.languageCode);
  std::string to_code = base::SysNSStringToUTF8(to_lang.languageCode);

  [translation_controller_ translatePageFromLanguage:from_lang
                                          toLanguage:to_lang
                                       userInitiated:YES];
  EXPECT_EQ(to_code, translate_client_->GetCurrentLang());

  TranslatePageInvocation invocation =
      translate_client_->GetLastTraslatePageInvocation();
  EXPECT_EQ(from_code, invocation.source_lang);
  EXPECT_EQ(to_code, invocation.target_lang);
  EXPECT_TRUE(invocation.triggered_from_menu);

  [translation_controller_ revertTranslation];
  EXPECT_EQ(translate_client_->GetPageLang(),
            translate_client_->GetCurrentLang());
}

}  // namespace ios_web_view
