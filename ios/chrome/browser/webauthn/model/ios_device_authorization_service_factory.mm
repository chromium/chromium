// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/ios_device_authorization_service_factory.h"

#import "base/feature_list.h"
#import "base/no_destructor.h"
#import "components/webauthn/core/browser/device_authorization/device_authorization_features.h"
#import "components/webauthn/core/browser/device_authorization/device_authorization_service_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/webauthn/model/ios_device_authorization_client.h"
#import "ios/chrome/common/channel_info.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
webauthn::DeviceAuthorizationService*
IOSDeviceAuthorizationServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<webauthn::DeviceAuthorizationService>(
          profile, /*create=*/true);
}

// static
IOSDeviceAuthorizationServiceFactory*
IOSDeviceAuthorizationServiceFactory::GetInstance() {
  static base::NoDestructor<IOSDeviceAuthorizationServiceFactory> instance;
  return instance.get();
}

IOSDeviceAuthorizationServiceFactory::IOSDeviceAuthorizationServiceFactory()
    : ProfileKeyedServiceFactoryIOS("DeviceAuthorizationService",
                                    ProfileSelection::kNoInstanceInIncognito) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

IOSDeviceAuthorizationServiceFactory::~IOSDeviceAuthorizationServiceFactory() =
    default;

std::unique_ptr<KeyedService>
IOSDeviceAuthorizationServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(
          webauthn::features::kFetchDeviceAuthorizationKeys)) {
    return nullptr;
  }

  return std::make_unique<webauthn::DeviceAuthorizationServiceImpl>(
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory(),
      std::make_unique<IOSDeviceAuthorizationClient>(), ::GetChannel());
}
