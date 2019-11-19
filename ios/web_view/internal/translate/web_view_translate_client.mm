// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/translate/web_view_translate_client.h"

#include <vector>

#include "base/logging.h"
#include "components/infobars/core/infobar.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/page_translated_details.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/language_detection_details.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"
#include "ios/web_view/internal/language/web_view_language_model_manager_factory.h"
#import "ios/web_view/internal/translate/cwv_translation_controller_internal.h"
#include "ios/web_view/internal/translate/web_view_translate_accept_languages_factory.h"
#include "ios/web_view/internal/translate/web_view_translate_ranker_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

WebViewTranslateClient::WebViewTranslateClient(web::WebState* web_state)
    : browser_state_(
          WebViewBrowserState::FromBrowserState(web_state->GetBrowserState())),
      translate_manager_(std::make_unique<translate::TranslateManager>(
          this,
          WebViewTranslateRankerFactory::GetForBrowserState(browser_state_),
          WebViewLanguageModelManagerFactory::GetForBrowserState(browser_state_)
              ->GetPrimaryModel())),
      translate_driver_(web_state,
                        web_state->GetNavigationManager(),
                        translate_manager_.get()) {
  web_state->AddObserver(this);
}

WebViewTranslateClient::~WebViewTranslateClient() = default;

void WebViewTranslateClient::TranslatePage(const std::string& source_lang,
                                           const std::string& target_lang,
                                           bool triggered_from_menu) {
  DCHECK(translate_manager_);
  translate_manager_->TranslatePage(source_lang, target_lang,
                                    triggered_from_menu);
}

void WebViewTranslateClient::RevertTranslation() {
  DCHECK(translate_manager_);
  translate_manager_->RevertTranslation();
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
  // Use recording browser state to share user settings.
  return browser_state_->GetRecordingBrowserState()->GetPrefs();
}

std::unique_ptr<translate::TranslatePrefs>
WebViewTranslateClient::GetTranslatePrefs() {
  return std::make_unique<translate::TranslatePrefs>(
      GetPrefs(), language::prefs::kAcceptLanguages, nullptr);
}

translate::TranslateAcceptLanguages*
WebViewTranslateClient::GetTranslateAcceptLanguages() {
  translate::TranslateAcceptLanguages* accept_languages =
      WebViewTranslateAcceptLanguagesFactory::GetForBrowserState(
          browser_state_);
  DCHECK(accept_languages);
  return accept_languages;
}

int WebViewTranslateClient::GetInfobarIconID() const {
  NOTREACHED();
  return 0;
}

bool WebViewTranslateClient::IsTranslatableURL(const GURL& url) {
  return !url.is_empty() && !url.SchemeIs(url::kFtpScheme);
}

void WebViewTranslateClient::ShowReportLanguageDetectionErrorUI(
    const GURL& report_url) {
  NOTREACHED();
}

// web::WebStateObserver implementation

void WebViewTranslateClient::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  // Translation process can be interrupted.
  // Destroying the TranslateManager now guarantees that it never has to deal
  // with nullptr WebState.
  translate_manager_.reset();
}

WEB_STATE_USER_DATA_KEY_IMPL(WebViewTranslateClient)

}  // namespace ios_web_view
