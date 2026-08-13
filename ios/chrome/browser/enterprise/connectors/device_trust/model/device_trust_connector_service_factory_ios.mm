// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_connector_service_factory_ios.h"

#import "base/no_destructor.h"
#import "components/enterprise/device_trust/core/device_trust_connector_service.h"
#import "components/enterprise/device_trust/prefs.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
enterprise_connectors::DeviceTrustConnectorService*
DeviceTrustConnectorServiceFactoryIOS::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<
          enterprise_connectors::DeviceTrustConnectorService>(profile,
                                                              /*create=*/true);
}

// static
DeviceTrustConnectorServiceFactoryIOS*
DeviceTrustConnectorServiceFactoryIOS::GetInstance() {
  static base::NoDestructor<DeviceTrustConnectorServiceFactoryIOS> instance;
  return instance.get();
}

DeviceTrustConnectorServiceFactoryIOS::DeviceTrustConnectorServiceFactoryIOS()
    : ProfileKeyedServiceFactoryIOS("DeviceTrustConnectorService",
                                    ProfileSelection::kNoInstanceInIncognito) {}

DeviceTrustConnectorServiceFactoryIOS::
    ~DeviceTrustConnectorServiceFactoryIOS() = default;

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
