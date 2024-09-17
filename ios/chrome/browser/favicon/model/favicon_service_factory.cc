// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/favicon/model/favicon_service_factory.h"

#include "base/no_destructor.h"
#include "components/favicon/core/favicon_service_impl.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
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
favicon::FaviconService* FaviconServiceFactory::GetForBrowserState(
    ProfileIOS* profile,
    ServiceAccessType access_type) {
  return GetForProfile(profile, access_type);
}

// static
favicon::FaviconService* FaviconServiceFactory::GetForProfile(
    ProfileIOS* profile,
    ServiceAccessType access_type) {
  if (!profile->IsOffTheRecord()) {
    return static_cast<favicon::FaviconService*>(
        GetInstance()->GetServiceForBrowserState(profile, true));
  } else if (access_type == ServiceAccessType::EXPLICIT_ACCESS) {
    return static_cast<favicon::FaviconService*>(
        GetInstance()->GetServiceForBrowserState(profile->GetOriginalProfile(),
                                                 true));
  }

  // ProfileIOS is OffTheRecord without access.
  NOTREACHED_IN_MIGRATION() << "ProfileIOS is OffTheRecord";
  return nullptr;
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
    : BrowserStateKeyedServiceFactory(
          "FaviconService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
}

FaviconServiceFactory::~FaviconServiceFactory() {
}

std::unique_ptr<KeyedService> FaviconServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildFaviconService(context);
}

bool FaviconServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios
