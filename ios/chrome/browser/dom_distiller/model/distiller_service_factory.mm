// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/dom_distiller/model/distiller_service_factory.h"

#import "components/dom_distiller/core/distiller.h"
#import "components/dom_distiller/core/distiller_url_fetcher.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
DistillerService* DistillerServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<DistillerService>(
      profile, /*create=*/true);
}

// static
DistillerServiceFactory* DistillerServiceFactory::GetInstance() {
  static base::NoDestructor<DistillerServiceFactory> instance;
  return instance.get();
}

DistillerServiceFactory::DistillerServiceFactory()
    : ProfileKeyedServiceFactoryIOS("DistillerService",
                                    ProfileSelection::kRedirectedInIncognito) {}

DistillerServiceFactory::~DistillerServiceFactory() {}

std::unique_ptr<KeyedService> DistillerServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  auto distiller_url_fetcher_factory =
      std::make_unique<dom_distiller::DistillerURLFetcherFactory>(
          context->GetSharedURLLoaderFactory());

  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  dom_distiller::proto::DomDistillerOptions options;
  return std::make_unique<DistillerService>(
      std::make_unique<dom_distiller::DistillerFactoryImpl>(
          std::move(distiller_url_fetcher_factory), options),
      profile->GetPrefs());
}
