// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/translate/web_view_translate_client.h"

#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "components/infobars/core/infobar.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/translate/core/browser/page_translated_details.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/language_detection_details.h"
#include "ios/web/public/browser_state.h"
#include "ios/web_view/internal/language/web_view_language_model_manager_factory.h"
#include "ios/web_view/internal/translate/web_view_translate_accept_languages_factory.h"
#include "ios/web_view/internal/translate/web_view_translate_ranker_factory.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// static
std::unique_ptr<WebViewTranslateClient> WebViewTranslateClient::Create(
    WebViewBrowserState* browser_state,
    web::WebState* web_state) {
  return std::make_unique<WebViewTranslateClient>(
      // Use recording browser state to share user settings in incognito.
      browser_state->GetRecordingBrowserState()->GetPrefs(),
      WebViewTranslateRankerFactory::GetForBrowserState(browser_state),
      WebViewLanguageModelManagerFactory::GetForBrowserState(browser_state)
          ->GetPrimaryModel(),
      web_state,
      WebViewTranslateAcceptLanguagesFactory::GetForBrowserState(
          browser_state));
}

WebViewTranslateClient::WebViewTranslateClient(
    PrefService* pref_service,
    translate::TranslateRanker* translate_ranker,
    language::LanguageModel* language_model,
    web::WebState* web_state,
    translate::TranslateAcceptLanguages* accept_languages)
    : pref_service_(pref_service),
      translate_manager_(this, translate_ranker, language_model),
      translate_driver_(web_state, &translate_manager_),
      accept_languages_(accept_languages) {
  DCHECK(pref_service_);
  DCHECK(accept_languages_);
}

WebViewTranslateClient::~WebViewTranslateClient() = default;

void WebViewTranslateClient::TranslatePage(const std::string& source_lang,
                                           const std::string& target_lang,
                                           bool triggered_from_menu) {
  translate_manager_.TranslatePage(source_lang, target_lang,
                                   triggered_from_menu);
}

void WebViewTranslateClient::RevertTranslation() {
  translate_manager_.RevertTranslation();
}

bool WebViewTranslateClient::RequestTranslationOffer() {
  if (translate_manager_.CanManuallyTranslate()) {
    translate_manager_.ShowTranslateUI();
    return true;
  } else {
    return false;
  }
}

// TranslateClient implementation:

std::unique_ptr<infobars::InfoBar> WebViewTranslateClient::CreateInfoBar(
    std::unique_ptr<translate::TranslateInfoBarDelegate> delegate) const {
  NOTREACHED();
  return nullptr;
}

bool WebViewTranslateClient::ShowTranslateUI(
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors::Type error_type,
    bool triggered_from_menu) {
  [translation_controller_ updateTranslateStep:step
                                sourceLanguage:source_language
                                targetLanguage:target_language
                                     errorType:error_type
                             triggeredFromMenu:triggered_from_menu];
  return true;
}

translate::IOSTranslateDriver* WebViewTranslateClient::GetTranslateDriver() {
  return &translate_driver_;
}

PrefService* WebViewTranslateClient::GetPrefs() {
  return pref_service_;
}

std::unique_ptr<translate::TranslatePrefs>
WebViewTranslateClient::GetTranslatePrefs() {
  return std::make_unique<translate::TranslatePrefs>(GetPrefs());
}

translate::TranslateAcceptLanguages*
WebViewTranslateClient::GetTranslateAcceptLanguages() {
  return accept_languages_;
}

int WebViewTranslateClient::GetInfobarIconID() const {
  NOTREACHED();
  return 0;
}

bool WebViewTranslateClient::IsTranslatableURL(const GURL& url) {
  return !url.is_empty() && !url.SchemeIs(url::kFtpScheme);
}

bool WebViewTranslateClient::IsAutofillAssistantRunning() const {
  return false;
}

}  // namespace ios_web_view
