// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/https_upgrades/model/https_upgrade_service_factory.h"

#import <utility>

#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/https_upgrades/model/https_upgrade_service_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

HttpsUpgradeServiceFactory::HttpsUpgradeServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "HttpsUpgradeService",
          ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(ios::HostContentSettingsMapFactory::GetInstance());
}

std::unique_ptr<KeyedService>
HttpsUpgradeServiceFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return std::make_unique<HttpsUpgradeServiceImpl>(
      profile->IsOffTheRecord(),
      ios::HostContentSettingsMapFactory::GetForProfile(profile).get());
}
