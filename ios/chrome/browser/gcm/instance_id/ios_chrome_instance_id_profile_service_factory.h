// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GCM_INSTANCE_ID_IOS_CHROME_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_GCM_INSTANCE_ID_IOS_CHROME_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace instance_id {
class InstanceIDProfileService;
}

// Singleton that owns all InstanceIDProfileService and associates them with
// ChromeBrowserState.
class IOSChromeInstanceIDProfileServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static instance_id::InstanceIDProfileService* GetForBrowserState(
      ChromeBrowserState* browser_state);

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

#endif  // IOS_CHROME_BROWSER_GCM_INSTANCE_ID_IOS_CHROME_INSTANCE_ID_PROFILE_SERVICE_FACTORY_H_
