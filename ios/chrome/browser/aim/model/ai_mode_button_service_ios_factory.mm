// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/ai_mode_button_service_ios_factory.h"

#import "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/browser/aim/model/ai_mode_button_service_ios.h"
#import "ios/chrome/browser/aim/model/ios_chrome_ai_mode_button_service_factory.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/image_fetcher/model/image_fetcher_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
AIModeButtonServiceIOS* AIModeButtonServiceIOSFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<AIModeButtonServiceIOS>(
      profile, /*create=*/true);
}

// static
AIModeButtonServiceIOSFactory* AIModeButtonServiceIOSFactory::GetInstance() {
  static base::NoDestructor<AIModeButtonServiceIOSFactory> instance;
  return instance.get();
}

AIModeButtonServiceIOSFactory::AIModeButtonServiceIOSFactory()
    : ProfileKeyedServiceFactoryIOS("AIModeButtonServiceIOS",
                                    ProfileSelection::kRedirectedInIncognito) {
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
  DependsOn(IOSChromeAimEligibilityServiceFactory::GetInstance());
  DependsOn(IOSChromeAiModeButtonServiceFactory::GetInstance());
  DependsOn(IOSChromeFaviconLoaderFactory::GetInstance());
  DependsOn(ios::FaviconServiceFactory::GetInstance());
  DependsOn(ImageFetcherServiceFactory::GetInstance());
}

AIModeButtonServiceIOSFactory::~AIModeButtonServiceIOSFactory() = default;

std::unique_ptr<KeyedService>
AIModeButtonServiceIOSFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<AIModeButtonServiceIOS>(
      ios::TemplateURLServiceFactory::GetForProfile(profile),
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile),
      IOSChromeAiModeButtonServiceFactory::GetForProfile(profile),
      IOSChromeFaviconLoaderFactory::GetForProfile(profile),
      ios::FaviconServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      ImageFetcherServiceFactory::GetForProfile(profile));
}
