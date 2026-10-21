// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_capabilities_manager_factory.h"

#import <utility>

#import "ios/chrome/browser/intelligence/bwg/model/gemini_capabilities_manager_impl.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"

namespace {

std::unique_ptr<KeyedService> BuildGeminiCapabilitiesManager(
    ProfileIOS* profile) {
  if (!IsAppSwitcherAISummarizationEnabled()) {
    return nullptr;
  }
  return std::make_unique<GeminiCapabilitiesManagerImpl>(
      profile, AuthenticationServiceFactory::GetForProfile(profile),
      GeminiServiceFactory::GetForProfile(profile));
}

}  // namespace

GeminiCapabilitiesManagerFactory::GeminiCapabilitiesManagerFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "GeminiCapabilitiesManager",
          ProfileSelection::kNoInstanceInIncognito) {
  DependsOn(AuthenticationServiceFactory::GetInstance());
  DependsOn(GeminiServiceFactory::GetInstance());
}

// static
GeminiCapabilitiesManagerFactory::TestingFactory
GeminiCapabilitiesManagerFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildGeminiCapabilitiesManager);
}

std::unique_ptr<KeyedService>
GeminiCapabilitiesManagerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildGeminiCapabilitiesManager(profile);
}
