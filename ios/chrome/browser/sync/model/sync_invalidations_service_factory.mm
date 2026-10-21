// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/sync_invalidations_service_factory.h"

#import <utility>

#import "components/gcm_driver/gcm_profile_service.h"
#import "components/gcm_driver/instance_id/instance_id_profile_service.h"
#import "components/sync/invalidations/sync_invalidations_service_impl.h"
#import "ios/chrome/browser/gcm/model/instance_id/ios_chrome_instance_id_profile_service_factory.h"
#import "ios/chrome/browser/gcm/model/ios_chrome_gcm_profile_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

SyncInvalidationsServiceFactory::SyncInvalidationsServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "SyncInvalidationsService") {
  DependsOn(IOSChromeGCMProfileServiceFactory::GetInstance());
  DependsOn(IOSChromeInstanceIDProfileServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SyncInvalidationsServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  gcm::GCMDriver* gcm_driver =
      IOSChromeGCMProfileServiceFactory::GetForProfile(profile)->driver();
  instance_id::InstanceIDDriver* instance_id_driver =
      IOSChromeInstanceIDProfileServiceFactory::GetForProfile(profile)
          ->driver();
  return std::make_unique<syncer::SyncInvalidationsServiceImpl>(
      gcm_driver, instance_id_driver);
}
