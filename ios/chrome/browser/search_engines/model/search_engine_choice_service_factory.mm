// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/model/search_engine_choice_service_factory.h"

#import <memory>

#import "base/check_deref.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/search_engines/model/ios_search_engine_choice_service_client.h"
#import "ios/chrome/browser/search_engines/model/template_url_prepopulate_data_resolver_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/browser_state.h"

namespace ios {

SearchEngineChoiceServiceFactory::SearchEngineChoiceServiceFactory()
    : ProfileKeyedServiceFactoryIOS("SearchEngineChoiceService",
                                    ProfileSelection::kRedirectedInIncognito) {
  DependsOn(ios::RegionalCapabilitiesServiceFactory::GetInstance());
  DependsOn(ios::TemplateURLPrepopulateDataResolverFactory::GetInstance());
}

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
  return GetInstance()
      ->GetServiceForProfileAs<search_engines::SearchEngineChoiceService>(
          profile, /*create=*/true);
}

std::unique_ptr<KeyedService>
SearchEngineChoiceServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<search_engines::SearchEngineChoiceService>(
      std::make_unique<IOSSearchEngineChoiceServiceClient>(),
      CHECK_DEREF(profile->GetPrefs()),
      GetApplicationContext()->GetLocalState(),
      CHECK_DEREF(
          ios::RegionalCapabilitiesServiceFactory::GetForProfile(profile)),
      CHECK_DEREF(ios::TemplateURLPrepopulateDataResolverFactory::GetForProfile(
          profile)));
}

}  // namespace ios
