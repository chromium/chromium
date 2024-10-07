// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/model/search_engine_choice_service_factory.h"

#import <memory>

#import "base/check_deref.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/browser_state.h"

namespace ios {

SearchEngineChoiceServiceFactory::SearchEngineChoiceServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SearchEngineChoiceServiceFactory",
          BrowserStateDependencyManager::GetInstance()) {}

SearchEngineChoiceServiceFactory::~SearchEngineChoiceServiceFactory() = default;

// static
SearchEngineChoiceServiceFactory*
SearchEngineChoiceServiceFactory::GetInstance() {
  static base::NoDestructor<SearchEngineChoiceServiceFactory> factory;
  return factory.get();
}

// static
search_engines::SearchEngineChoiceService*
SearchEngineChoiceServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<search_engines::SearchEngineChoiceService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

std::unique_ptr<KeyedService>
SearchEngineChoiceServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<search_engines::SearchEngineChoiceService>(
      CHECK_DEREF(profile->GetPrefs()),
      GetApplicationContext()->GetLocalState(),
      /*is_profile_elibile_for_dse_guest_propagation=*/false,
      GetApplicationContext()->GetVariationsService());
}

web::BrowserState* SearchEngineChoiceServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

}  // namespace ios
