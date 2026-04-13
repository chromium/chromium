// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"

#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_impl.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

class GeminiService;

namespace {

std::unique_ptr<KeyedService> BuildGeminiService(ProfileIOS* profile) {
  if (!IsPageActionMenuEnabled()) {
    return nullptr;
  }
  return std::make_unique<GeminiServiceImpl>(
      profile, AuthenticationServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile), profile->GetPrefs(),
      OptimizationGuideServiceFactory::GetForProfile(profile));
}

}  // namespace

// static
GeminiService* GeminiServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<GeminiService>(profile,
                                                              /*create=*/true);
}

// static
GeminiServiceFactory* GeminiServiceFactory::GetInstance() {
  static base::NoDestructor<GeminiServiceFactory> instance;
  return instance.get();
}

GeminiServiceFactory::GeminiServiceFactory()
    : ProfileKeyedServiceFactoryIOS("GeminiService") {
  DependsOn(AuthenticationServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

GeminiServiceFactory::~GeminiServiceFactory() = default;

// static
GeminiServiceFactory::TestingFactory GeminiServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildGeminiService);
}

std::unique_ptr<KeyedService> GeminiServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildGeminiService(profile);
}
