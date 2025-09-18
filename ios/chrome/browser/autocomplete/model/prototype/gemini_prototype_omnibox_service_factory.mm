// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/prototype/gemini_prototype_omnibox_service_factory.h"

#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/autocomplete/model/prototype/gemini_prototype_omnibox_service_ios.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
GeminiPrototypeOmniboxService*
GeminiPrototypeOmniboxServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<GeminiPrototypeOmniboxService*>(
      GetInstance()->GetServiceForProfileAs<GeminiPrototypeOmniboxService>(
          profile, /*create=*/true));
}

// static
GeminiPrototypeOmniboxServiceFactory*
GeminiPrototypeOmniboxServiceFactory::GetInstance() {
  static base::NoDestructor<GeminiPrototypeOmniboxServiceFactory> instance;
  return instance.get();
}

GeminiPrototypeOmniboxServiceFactory::GeminiPrototypeOmniboxServiceFactory()
    : ProfileKeyedServiceFactoryIOS("GeminiPrototypeOmniboxService") {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

GeminiPrototypeOmniboxServiceFactory::~GeminiPrototypeOmniboxServiceFactory() =
    default;

std::unique_ptr<KeyedService>
GeminiPrototypeOmniboxServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  // This service is not available in incognito.
  if (!omnibox::IsGeminiPrototypeProviderEnabled() ||
      profile->IsOffTheRecord()) {
    return nullptr;
  }
  return std::make_unique<GeminiPrototypeOmniboxServiceIOS>(profile);
}
