// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/invalidation/model/ios_chrome_profile_invalidation_provider_factory.h"

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "components/gcm_driver/gcm_profile_service.h"
#import "components/gcm_driver/instance_id/instance_id_profile_service.h"
#import "components/invalidation/profile_invalidation_provider.h"
#import "components/prefs/pref_registry.h"
#import "ios/chrome/browser/gcm/model/instance_id/ios_chrome_instance_id_profile_service_factory.h"
#import "ios/chrome/browser/gcm/model/ios_chrome_gcm_profile_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

using invalidation::ProfileInvalidationProvider;

IOSChromeProfileInvalidationProviderFactory::
    IOSChromeProfileInvalidationProviderFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "InvalidationService") {
  DependsOn(IOSChromeGCMProfileServiceFactory::GetInstance());
  DependsOn(IOSChromeInstanceIDProfileServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSChromeProfileInvalidationProviderFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ProfileInvalidationProvider>();
}
