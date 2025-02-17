// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/model/template_url_prepopulate_data_resolver_factory.h"

#import <memory>

#import "base/check_deref.h"
#import "components/search_engines/template_url_prepopulate_data_resolver.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/browser_state.h"

namespace ios {

TemplateURLPrepopulateDataResolverFactory::
    TemplateURLPrepopulateDataResolverFactory()
    : ProfileKeyedServiceFactoryIOS(
          "TemplateURLPrepopulateDataResolver",
          // Service intended as a helper / forwarder to other services. So it
          // should be as available as possible, other services are responsible
          // of forwarding to the parent profile as they see fit.
          ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(RegionalCapabilitiesServiceFactory::GetInstance());
}

TemplateURLPrepopulateDataResolverFactory::
    ~TemplateURLPrepopulateDataResolverFactory() = default;

// static
TemplateURLPrepopulateDataResolverFactory*
TemplateURLPrepopulateDataResolverFactory::GetInstance() {
  static base::NoDestructor<TemplateURLPrepopulateDataResolverFactory> factory;
  return factory.get();
}

// static
TemplateURLPrepopulateData::Resolver*
TemplateURLPrepopulateDataResolverFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<TemplateURLPrepopulateData::Resolver>(
          profile, /*create=*/true);
}

std::unique_ptr<KeyedService>
TemplateURLPrepopulateDataResolverFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  return std::make_unique<TemplateURLPrepopulateData::Resolver>(
      CHECK_DEREF(profile->GetPrefs()),
      CHECK_DEREF(
          ios::RegionalCapabilitiesServiceFactory::GetForProfile(profile)));
}

}  // namespace ios
