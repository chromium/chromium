// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_IOS_USER_EVENT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_IOS_USER_EVENT_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace syncer {
class UserEventService;
}  // namespace syncer

// Singleton that associates UserEventServices to ProfileIOS.
class IOSUserEventServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static syncer::UserEventService* GetForProfile(ProfileIOS* profile);
  static IOSUserEventServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSUserEventServiceFactory>;

  IOSUserEventServiceFactory();
  ~IOSUserEventServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_IOS_USER_EVENT_SERVICE_FACTORY_H_
