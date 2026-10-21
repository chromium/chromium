// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/ios_chrome_ai_mode_button_service_factory.h"

#import <utility>

#import "components/search_engines/ai_mode_button_service.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

IOSChromeAiModeButtonServiceFactory::IOSChromeAiModeButtonServiceFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "AiModeButtonService",
          ProfileSelection::kRedirectedInIncognito) {
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSChromeAiModeButtonServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  TemplateURLService* template_url_service =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  if (!template_url_service) {
    return nullptr;
  }
  return std::make_unique<AiModeButtonService>(template_url_service);
}
