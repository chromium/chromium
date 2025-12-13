// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_service_factory.h"

#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

class BwgService;

namespace {

std::unique_ptr<KeyedService> BuildBwgService(ProfileIOS* profile) {
  if (!IsPageActionMenuEnabled()) {
    return nullptr;
  }
  return std::make_unique<BwgService>(
      profile, AuthenticationServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile), profile->GetPrefs(),
      OptimizationGuideServiceFactory::GetForProfile(profile));
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
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

BwgServiceFactory::~BwgServiceFactory() = default;

// static
BwgServiceFactory::TestingFactory BwgServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildBwgService);
}

std::unique_ptr<KeyedService> BwgServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildBwgService(profile);
}
