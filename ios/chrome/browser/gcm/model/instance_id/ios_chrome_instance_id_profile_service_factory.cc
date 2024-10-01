// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/gcm/model/instance_id/ios_chrome_instance_id_profile_service_factory.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/gcm/model/ios_chrome_gcm_profile_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
instance_id::InstanceIDProfileService*
IOSChromeInstanceIDProfileServiceFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
instance_id::InstanceIDProfileService*
IOSChromeInstanceIDProfileServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<instance_id::InstanceIDProfileService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
IOSChromeInstanceIDProfileServiceFactory*
IOSChromeInstanceIDProfileServiceFactory::GetInstance() {
  static base::NoDestructor<IOSChromeInstanceIDProfileServiceFactory> instance;
  return instance.get();
}

IOSChromeInstanceIDProfileServiceFactory::
    IOSChromeInstanceIDProfileServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "InstanceIDProfileService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IOSChromeGCMProfileServiceFactory::GetInstance());
}

IOSChromeInstanceIDProfileServiceFactory::
    ~IOSChromeInstanceIDProfileServiceFactory() {}

std::unique_ptr<KeyedService>
IOSChromeInstanceIDProfileServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  DCHECK(!context->IsOffTheRecord());

  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<instance_id::InstanceIDProfileService>(
      IOSChromeGCMProfileServiceFactory::GetForProfile(profile)->driver(),
      profile->IsOffTheRecord());
}
