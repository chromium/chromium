// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/ios_device_authorization_service_factory.h"

#import <utility>

#import "base/feature_list.h"
#import "components/webauthn/core/browser/device_authorization/device_authorization_features.h"
#import "components/webauthn/core/browser/device_authorization/device_authorization_service_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/webauthn/model/ios_device_authorization_client.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

IOSDeviceAuthorizationServiceFactory::IOSDeviceAuthorizationServiceFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "DeviceAuthorizationService",
          ProfileSelection::kNoInstanceInIncognito) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(IOSPasskeyModelFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSDeviceAuthorizationServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(
          webauthn::features::kFetchDeviceAuthorizationKeys)) {
    return nullptr;
  }

  return std::make_unique<webauthn::DeviceAuthorizationServiceImpl>(
      IdentityManagerFactory::GetForProfile(profile),
      IOSPasskeyModelFactory::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory(),
      std::make_unique<IOSDeviceAuthorizationClient>(), ::GetChannel());
}
