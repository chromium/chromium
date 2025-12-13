// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_IOS_CHROME_TAB_RESTORE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_IOS_CHROME_TAB_RESTORE_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace sessions {
class TabRestoreService;
}

// Singleton that owns all TabRestoreServices and associates them with
// ProfileIOS.
class IOSChromeTabRestoreServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static sessions::TabRestoreService* GetForProfile(ProfileIOS* profile);
  static IOSChromeTabRestoreServiceFactory* GetInstance();

  // Returns the default factory used to build TabRestoreServices. Can be
  // registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<IOSChromeTabRestoreServiceFactory>;

  IOSChromeTabRestoreServiceFactory();
  ~IOSChromeTabRestoreServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_IOS_CHROME_TAB_RESTORE_SERVICE_FACTORY_H_
