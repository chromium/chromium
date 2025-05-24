// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GCM_MODEL_INSTANCE_ID_IOS_CHROME_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_GCM_MODEL_INSTANCE_ID_IOS_CHROME_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace instance_id {
class InstanceIDProfileService;
}

// Singleton that owns all InstanceIDProfileService and associates them with
// ProfileIOS.
class IOSChromeInstanceIDProfileServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static instance_id::InstanceIDProfileService* GetForProfile(
      ProfileIOS* profile);
  static IOSChromeInstanceIDProfileServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSChromeInstanceIDProfileServiceFactory>;

  IOSChromeInstanceIDProfileServiceFactory();
  ~IOSChromeInstanceIDProfileServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_GCM_MODEL_INSTANCE_ID_IOS_CHROME_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_
