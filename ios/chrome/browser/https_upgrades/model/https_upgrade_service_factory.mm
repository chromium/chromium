// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/https_upgrades/model/https_upgrade_service_factory.h"

#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
HttpsUpgradeService* HttpsUpgradeServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<HttpsUpgradeService>(
      profile, /*create=*/true);
}

// static
HttpsUpgradeServiceFactory* HttpsUpgradeServiceFactory::GetInstance() {
  static base::NoDestructor<HttpsUpgradeServiceFactory> instance;
  return instance.get();
}

HttpsUpgradeServiceFactory::HttpsUpgradeServiceFactory()
    : ProfileKeyedServiceFactoryIOS("HttpsUpgradeService",
                                    ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(ios::HostContentSettingsMapFactory::GetInstance());
}

HttpsUpgradeServiceFactory::~HttpsUpgradeServiceFactory() = default;

std::unique_ptr<KeyedService>
HttpsUpgradeServiceFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return std::make_unique<HttpsUpgradeServiceImpl>(
      profile->IsOffTheRecord(),
      ios::HostContentSettingsMapFactory::GetForProfile(profile));
}
