// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/gcm/model/instance_id/ios_chrome_instance_id_profile_service_factory.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "ios/chrome/browser/gcm/model/ios_chrome_gcm_profile_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
instance_id::InstanceIDProfileService*
IOSChromeInstanceIDProfileServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<instance_id::InstanceIDProfileService>(
          profile, /*create=*/true);
}

// static
IOSChromeInstanceIDProfileServiceFactory*
IOSChromeInstanceIDProfileServiceFactory::GetInstance() {
  static base::NoDestructor<IOSChromeInstanceIDProfileServiceFactory> instance;
  return instance.get();
}

IOSChromeInstanceIDProfileServiceFactory::
    IOSChromeInstanceIDProfileServiceFactory()
    : ProfileKeyedServiceFactoryIOS("InstanceIDProfileService") {
  DependsOn(IOSChromeGCMProfileServiceFactory::GetInstance());
}

IOSChromeInstanceIDProfileServiceFactory::
    ~IOSChromeInstanceIDProfileServiceFactory() {}

std::unique_ptr<KeyedService>
IOSChromeInstanceIDProfileServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  DCHECK(!profile->IsOffTheRecord());

  return std::make_unique<instance_id::InstanceIDProfileService>(
      IOSChromeGCMProfileServiceFactory::GetForProfile(profile)->driver(),
      profile->IsOffTheRecord());
}
