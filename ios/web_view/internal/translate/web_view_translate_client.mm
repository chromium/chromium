// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/translate/web_view_translate_client.h"

#import <vector>

#import "base/check.h"
#import "base/notreached.h"
#import "components/infobars/core/infobar.h"
#import "components/language/core/browser/language_model_manager.h"
#import "components/language/core/browser/pref_names.h"
#import "components/translate/core/browser/page_translated_details.h"
#import "components/translate/core/browser/translate_infobar_delegate.h"
#import "components/translate/core/browser/translate_step.h"
#import "ios/web/public/browser_state.h"
#import "ios/web_view/internal/language/web_view_accept_languages_service_factory.h"
#import "ios/web_view/internal/language/web_view_language_model_manager_factory.h"
#import "ios/web_view/internal/language/web_view_url_language_histogram_factory.h"
#import "ios/web_view/internal/translate/web_view_translate_ranker_factory.h"
#import "url/gurl.h"

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
      WebViewUrlLanguageHistogramFactory::GetForBrowserState(browser_state),
      web_state,
      WebViewAcceptLanguagesServiceFactory::GetForBrowserState(browser_state));
}

WebViewTranslateClient::WebViewTranslateClient(
    PrefService* pref_service,
    translate::TranslateRanker* translate_ranker,
    language::LanguageModel* language_model,
    language::UrlLanguageHistogram* url_language_histogram,
    web::WebState* web_state,
    language::AcceptLanguagesService* accept_languages)
    : pref_service_(pref_service),
      translate_driver_(web_state,
                        /*language_detection_model_service=*/nullptr),
      translate_manager_(this, translate_ranker, language_model),
      accept_languages_(accept_languages) {
  DCHECK(pref_service_);
  DCHECK(accept_languages_);
  translate_driver_.Initialize(url_language_histogram, &translate_manager_);
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
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

bool WebViewTranslateClient::ShowTranslateUI(
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors error_type,
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

language::AcceptLanguagesService*
WebViewTranslateClient::GetAcceptLanguagesService() {
  return accept_languages_;
}

bool WebViewTranslateClient::IsTranslatableURL(const GURL& url) {
  return !url.is_empty() && !url.SchemeIs(url::kFtpScheme);
}

}  // namespace ios_web_view
