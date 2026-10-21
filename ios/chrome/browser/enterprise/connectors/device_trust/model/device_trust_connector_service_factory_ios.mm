// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_connector_service_factory_ios.h"

#import <utility>

#import "components/enterprise/device_trust/core/device_trust_connector_service.h"
#import "components/enterprise/device_trust/prefs.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

DeviceTrustConnectorServiceFactoryIOS::DeviceTrustConnectorServiceFactoryIOS(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "DeviceTrustConnectorService",
          ProfileSelection::kNoInstanceInIncognito) {}

void DeviceTrustConnectorServiceFactoryIOS::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  enterprise_connectors::RegisterDeviceTrustConnectorProfilePrefs(registry);
}

std::unique_ptr<KeyedService>
DeviceTrustConnectorServiceFactoryIOS::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<enterprise_connectors::DeviceTrustConnectorService>(
      profile->GetPrefs());
}
