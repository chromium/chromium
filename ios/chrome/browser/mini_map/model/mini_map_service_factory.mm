// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mini_map/model/mini_map_service_factory.h"

#import "base/check.h"
#import "base/feature_list.h"
#import "ios/chrome/browser/mini_map/model/mini_map_service.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

// static
MiniMapService* MiniMapServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<MiniMapService*>(
      GetInstance()->GetServiceForProfileAs<MiniMapService>(profile,
                                                            /*create=*/true));
}

// static
MiniMapServiceFactory* MiniMapServiceFactory::GetInstance() {
  static base::NoDestructor<MiniMapServiceFactory> instance;
  return instance.get();
}

MiniMapServiceFactory::MiniMapServiceFactory()
    : ProfileKeyedServiceFactoryIOS("MiniMapService") {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
}

MiniMapServiceFactory::~MiniMapServiceFactory() = default;

std::unique_ptr<KeyedService> MiniMapServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  CHECK(!profile->IsOffTheRecord());
  CHECK(base::FeatureList::IsEnabled(kIOSMiniMapUniversalLink));

  return std::make_unique<MiniMapService>(
      profile->GetPrefs(),
      ios::TemplateURLServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile));
}
