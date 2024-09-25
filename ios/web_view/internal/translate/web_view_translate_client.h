// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_TRANSLATE_WEB_VIEW_TRANSLATE_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_TRANSLATE_WEB_VIEW_TRANSLATE_CLIENT_H_

#include <memory>
#include <string>

#import "base/memory/raw_ptr.h"
#include "components/language/core/browser/accept_languages_service.h"
#include "components/language/core/browser/language_model.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/browser/translate_ranker.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/translate_errors.h"
#import "components/translate/ios/browser/ios_translate_driver.h"
#import "ios/web/public/web_state.h"
#import "ios/web_view/internal/translate/cwv_translation_controller_internal.h"
#include "ios/web_view/internal/web_view_browser_state.h"

namespace ios_web_view {

class WebViewTranslateClient : public translate::TranslateClient {
 public:
  static std::unique_ptr<WebViewTranslateClient> Create(
      WebViewBrowserState* browser_state,
      web::WebState* web_state);

  explicit WebViewTranslateClient(
      PrefService* pref_service,
      translate::TranslateRanker* translate_ranker,
      language::LanguageModel* language_model,
      language::UrlLanguageHistogram* url_language_histogram,
      web::WebState* web_state,
      language::AcceptLanguagesService* accept_languages);

  WebViewTranslateClient(const WebViewTranslateClient&) = delete;
  WebViewTranslateClient& operator=(const WebViewTranslateClient&) = delete;

  ~WebViewTranslateClient() override;

  // This |controller| is assumed to outlive this WebViewTranslateClient.
  void set_translation_controller(CWVTranslationController* controller) {
    translation_controller_ = controller;
  }

  // Performs translation from |source_lang| to |target_lang|.
  // |trigged_from_menu| indicates if a direct result of user.
  // Marked virtual to allow for testing.
  virtual void TranslatePage(const std::string& source_lang,
                             const std::string& target_lang,
                             bool triggered_from_menu);

  // Reverts previous translations back to original language.
  // Marked virtual to allow for testing.
  virtual void RevertTranslation();

  // Attempts to initiate a manual translation flow.
  // Returns boolean indicating if translation can be offered.
  // Marked virtual to allow for testing.
  virtual bool RequestTranslationOffer();

  // TranslateClient implementation.
  translate::IOSTranslateDriver* GetTranslateDriver() override;
  PrefService* GetPrefs() override;
  std::unique_ptr<translate::TranslatePrefs> GetTranslatePrefs() override;
  language::AcceptLanguagesService* GetAcceptLanguagesService() override;
  std::unique_ptr<infobars::InfoBar> CreateInfoBar(
      std::unique_ptr<translate::TranslateInfoBarDelegate> delegate)
      const override;
  bool ShowTranslateUI(translate::TranslateStep step,
                       const std::string& source_language,
                       const std::string& target_language,
                       translate::TranslateErrors error_type,
                       bool triggered_from_menu) override;
  bool IsTranslatableURL(const GURL& url) override;

 private:
  raw_ptr<PrefService> pref_service_;
  translate::IOSTranslateDriver translate_driver_;
  translate::TranslateManager translate_manager_;
  language::AcceptLanguagesService* accept_languages_;

  // ObjC class that wraps this class.
  __weak CWVTranslationController* translation_controller_ = nil;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_TRANSLATE_WEB_VIEW_TRANSLATE_CLIENT_H_
