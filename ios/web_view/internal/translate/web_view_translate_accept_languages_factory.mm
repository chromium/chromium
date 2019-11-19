// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/translate/web_view_translate_accept_languages_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "ios/web_view/internal/web_view_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// TranslateAcceptLanguagesService is a thin container for
// TranslateAcceptLanguages to enable associating it with a BrowserState.
class TranslateAcceptLanguagesService : public KeyedService {
 public:
  explicit TranslateAcceptLanguagesService(PrefService* prefs);
  ~TranslateAcceptLanguagesService() override;

  // Returns the associated TranslateAcceptLanguages.
  translate::TranslateAcceptLanguages& accept_languages() {
    return accept_languages_;
  }

 private:
  translate::TranslateAcceptLanguages accept_languages_;

  DISALLOW_COPY_AND_ASSIGN(TranslateAcceptLanguagesService);
};

TranslateAcceptLanguagesService::TranslateAcceptLanguagesService(
    PrefService* prefs)
    : accept_languages_(prefs, language::prefs::kAcceptLanguages) {}

TranslateAcceptLanguagesService::~TranslateAcceptLanguagesService() = default;

}  // namespace

namespace ios_web_view {

// static
WebViewTranslateAcceptLanguagesFactory*
WebViewTranslateAcceptLanguagesFactory::GetInstance() {
  static base::NoDestructor<WebViewTranslateAcceptLanguagesFactory> instance;
  return instance.get();
}

// static
translate::TranslateAcceptLanguages*
WebViewTranslateAcceptLanguagesFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  TranslateAcceptLanguagesService* service =
      static_cast<TranslateAcceptLanguagesService*>(
          GetInstance()->GetServiceForBrowserState(browser_state, true));
  return &service->accept_languages();
}

WebViewTranslateAcceptLanguagesFactory::WebViewTranslateAcceptLanguagesFactory()
    : BrowserStateKeyedServiceFactory(
          "TranslateAcceptLanguagesService",
          BrowserStateDependencyManager::GetInstance()) {}

WebViewTranslateAcceptLanguagesFactory::
    ~WebViewTranslateAcceptLanguagesFactory() {}

std::unique_ptr<KeyedService>
WebViewTranslateAcceptLanguagesFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return std::make_unique<TranslateAcceptLanguagesService>(
      browser_state->GetPrefs());
}

web::BrowserState* WebViewTranslateAcceptLanguagesFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return browser_state->GetRecordingBrowserState();
}

}  // namespace ios_web_view
