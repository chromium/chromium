// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_service_factory.h"

#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"

class BwgService;

namespace {

std::unique_ptr<KeyedService> BuildBwgService(web::BrowserState* context) {
  if (!IsPageActionMenuEnabled()) {
    return nullptr;
  }
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<BwgService>(
      AuthenticationServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile), profile->GetPrefs());
}

}  // namespace

// static
BwgService* BwgServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<BwgService>(profile,
                                                           /*create=*/true);
}

// static
BwgServiceFactory* BwgServiceFactory::GetInstance() {
  static base::NoDestructor<BwgServiceFactory> instance;
  return instance.get();
}

BwgServiceFactory::BwgServiceFactory()
    : ProfileKeyedServiceFactoryIOS("BwgService") {
  DependsOn(AuthenticationServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

BwgServiceFactory::~BwgServiceFactory() = default;

// static
BrowserStateKeyedServiceFactory::TestingFactory
BwgServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildBwgService);
}

std::unique_ptr<KeyedService> BwgServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildBwgService(context);
}
