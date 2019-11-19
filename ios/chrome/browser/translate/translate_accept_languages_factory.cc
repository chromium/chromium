// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/translate_accept_languages_factory.h"

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

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

TranslateAcceptLanguagesService::~TranslateAcceptLanguagesService() {
}

}  // namespace

// static
TranslateAcceptLanguagesFactory*
TranslateAcceptLanguagesFactory::GetInstance() {
  static base::NoDestructor<TranslateAcceptLanguagesFactory> instance;
  return instance.get();
}

// static
translate::TranslateAcceptLanguages*
TranslateAcceptLanguagesFactory::GetForBrowserState(
    ios::ChromeBrowserState* state) {
  TranslateAcceptLanguagesService* service =
      static_cast<TranslateAcceptLanguagesService*>(
          GetInstance()->GetServiceForBrowserState(state, true));
  return &service->accept_languages();
}

TranslateAcceptLanguagesFactory::TranslateAcceptLanguagesFactory()
    : BrowserStateKeyedServiceFactory(
          "TranslateAcceptLanguagesService",
          BrowserStateDependencyManager::GetInstance()) {
}

TranslateAcceptLanguagesFactory::~TranslateAcceptLanguagesFactory() {
}

std::unique_ptr<KeyedService>
TranslateAcceptLanguagesFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<TranslateAcceptLanguagesService>(
      browser_state->GetPrefs());
}

web::BrowserState* TranslateAcceptLanguagesFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
