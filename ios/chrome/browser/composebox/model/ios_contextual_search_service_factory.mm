// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/model/ios_contextual_search_service_factory.h"

#import "components/application_locale_storage/application_locale_storage.h"
#import "components/contextual_search/contextual_search_service.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/variations/variations_client.h"
#import "ios/chrome/browser/composebox/model/ios_contextual_search_service.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/variations/model/client/variations_client_service.h"
#import "ios/chrome/browser/variations/model/client/variations_client_service_factory.h"
#import "ios/chrome/common/channel_info.h"

// static
contextual_search::ContextualSearchService*
ContextualSearchServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<contextual_search::ContextualSearchService*>(
      GetInstance()
          ->GetServiceForProfileAs<contextual_search::ContextualSearchService>(
              profile, /*create=*/true));
}

// static
ContextualSearchServiceFactory* ContextualSearchServiceFactory::GetInstance() {
  static base::NoDestructor<ContextualSearchServiceFactory> instance;
  return instance.get();
}

ContextualSearchServiceFactory::ContextualSearchServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ContextualSearchService",
                                    ProfileSelection::kOwnInstanceInIncognito,
                                    TestingCreation::kCreateService) {
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(VariationsClientServiceFactory::GetInstance());
}

ContextualSearchServiceFactory::~ContextualSearchServiceFactory() = default;

std::unique_ptr<KeyedService>
ContextualSearchServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  auto* variations_client_service =
      VariationsClientServiceFactory::GetForProfile(profile);
  return std::make_unique<IOSContextualSearchService>(
      IdentityManagerFactory::GetForProfile(profile),
      GetApplicationContext()->GetSharedURLLoaderFactory(),
      ios::TemplateURLServiceFactory::GetForProfile(profile),
      static_cast<variations::VariationsClient*>(variations_client_service),
      ::GetChannel(),
      GetApplicationContext()->GetApplicationLocaleStorage()->Get());
}
