// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/translate_ranker_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/translate/core/browser/translate_ranker_impl.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

namespace translate {

// static
TranslateRankerFactory* TranslateRankerFactory::GetInstance() {
  static base::NoDestructor<TranslateRankerFactory> instance;
  return instance.get();
}

// static
translate::TranslateRanker* TranslateRankerFactory::GetForBrowserState(
    ChromeBrowserState* state) {
  return static_cast<TranslateRanker*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

TranslateRankerFactory::TranslateRankerFactory()
    : BrowserStateKeyedServiceFactory(
          "TranslateRankerService",
          BrowserStateDependencyManager::GetInstance()) {}

TranslateRankerFactory::~TranslateRankerFactory() {}

std::unique_ptr<KeyedService> TranslateRankerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<TranslateRankerImpl>(
      TranslateRankerImpl::GetModelPath(browser_state->GetStatePath()),
      TranslateRankerImpl::GetModelURL(),
      GetApplicationContext()->GetUkmRecorder());
}

web::BrowserState* TranslateRankerFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

}  // namespace translate
