// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/gcm/model/instance_id/ios_chrome_instance_id_profile_service_factory.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "ios/chrome/browser/gcm/model/ios_chrome_gcm_profile_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

IOSChromeInstanceIDProfileServiceFactory::
    IOSChromeInstanceIDProfileServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "InstanceIDProfileService") {
  DependsOn(IOSChromeGCMProfileServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSChromeInstanceIDProfileServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  DCHECK(!profile->IsOffTheRecord());

  return std::make_unique<instance_id::InstanceIDProfileService>(
      IOSChromeGCMProfileServiceFactory::GetForProfile(profile)->driver(),
      profile->IsOffTheRecord());
}
