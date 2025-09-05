// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/search_engines/model/template_url_fetcher_factory.h"

#include "components/search_engines/template_url_fetcher.h"
#include "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

// static
TemplateURLFetcher* TemplateURLFetcherFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<TemplateURLFetcher>(
      profile, /*create=*/true);
}

// static
TemplateURLFetcherFactory* TemplateURLFetcherFactory::GetInstance() {
  static base::NoDestructor<TemplateURLFetcherFactory> instance;
  return instance.get();
}

TemplateURLFetcherFactory::TemplateURLFetcherFactory()
    : ProfileKeyedServiceFactoryIOS("TemplateURLFetcher",
                                    ProfileSelection::kRedirectedInIncognito) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

TemplateURLFetcherFactory::~TemplateURLFetcherFactory() = default;

std::unique_ptr<KeyedService>
TemplateURLFetcherFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return std::make_unique<TemplateURLFetcher>(
      TemplateURLServiceFactory::GetForProfile(profile));
}

}  // namespace ios
