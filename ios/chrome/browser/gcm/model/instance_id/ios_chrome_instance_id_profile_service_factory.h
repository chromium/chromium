// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GCM_MODEL_INSTANCE_ID_IOS_CHROME_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_GCM_MODEL_INSTANCE_ID_IOS_CHROME_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_

#include <memory>

#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all InstanceIDProfileService and associates them with
// ProfileIOS.
class IOSChromeInstanceIDProfileServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSChromeInstanceIDProfileServiceFactory,
          instance_id::InstanceIDProfileService> {
 public:
  explicit IOSChromeInstanceIDProfileServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_GCM_MODEL_INSTANCE_ID_IOS_CHROME_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_
