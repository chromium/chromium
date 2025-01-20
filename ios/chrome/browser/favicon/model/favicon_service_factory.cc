// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/favicon/model/favicon_service_factory.h"

#include "components/favicon/core/favicon_service_impl.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ios/chrome/browser/favicon/model/favicon_client_impl.h"
#include "ios/chrome/browser/history/model/history_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

std::unique_ptr<KeyedService> BuildFaviconService(web::BrowserState* context) {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<favicon::FaviconServiceImpl>(
      std::make_unique<FaviconClientImpl>(),
      ios::HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS));
}

}  // namespace

namespace ios {

// static
favicon::FaviconService* FaviconServiceFactory::GetForProfile(
    ProfileIOS* profile,
    ServiceAccessType access_type) {
  if (!profile->IsOffTheRecord()) {
    return GetInstance()->GetServiceForProfileAs<favicon::FaviconService>(
        profile, /*create=*/true);
  } else if (access_type == ServiceAccessType::EXPLICIT_ACCESS) {
    return GetInstance()->GetServiceForProfileAs<favicon::FaviconService>(
        profile->GetOriginalProfile(),
        /*create=*/true);
  }

  // ProfileIOS is OffTheRecord without access.
  NOTREACHED() << "ProfileIOS is OffTheRecord";
}

// static
FaviconServiceFactory* FaviconServiceFactory::GetInstance() {
  static base::NoDestructor<FaviconServiceFactory> instance;
  return instance.get();
}

// static
BrowserStateKeyedServiceFactory::TestingFactory
FaviconServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildFaviconService);
}

FaviconServiceFactory::FaviconServiceFactory()
    : ProfileKeyedServiceFactoryIOS("FaviconService",
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
}

FaviconServiceFactory::~FaviconServiceFactory() = default;

std::unique_ptr<KeyedService> FaviconServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildFaviconService(context);
}

}  // namespace ios
