// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GCM_MODEL_INSTANCE_ID_IOS_CHROME_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_GCM_MODEL_INSTANCE_ID_IOS_CHROME_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace instance_id {
class InstanceIDProfileService;
}

// Singleton that owns all InstanceIDProfileService and associates them with
// ProfileIOS.
class IOSChromeInstanceIDProfileServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static instance_id::InstanceIDProfileService* GetForBrowserState(
      ProfileIOS* profile);

  static instance_id::InstanceIDProfileService* GetForProfile(
      ProfileIOS* profile);
  static IOSChromeInstanceIDProfileServiceFactory* GetInstance();

  IOSChromeInstanceIDProfileServiceFactory(
      const IOSChromeInstanceIDProfileServiceFactory&) = delete;
  IOSChromeInstanceIDProfileServiceFactory& operator=(
      const IOSChromeInstanceIDProfileServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSChromeInstanceIDProfileServiceFactory>;

  IOSChromeInstanceIDProfileServiceFactory();
  ~IOSChromeInstanceIDProfileServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_GCM_MODEL_INSTANCE_ID_IOS_CHROME_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_
