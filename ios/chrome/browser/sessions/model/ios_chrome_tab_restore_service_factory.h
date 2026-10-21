// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_IOS_CHROME_TAB_RESTORE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_IOS_CHROME_TAB_RESTORE_SERVICE_FACTORY_H_

#include <memory>

#include "components/sessions/core/tab_restore_service.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all TabRestoreServices and associates them with
// ProfileIOS.
class IOSChromeTabRestoreServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSChromeTabRestoreServiceFactory,
          sessions::TabRestoreService> {
 public:
  explicit IOSChromeTabRestoreServiceFactory(PassKey key);

  // Returns the default factory used to build TabRestoreServices. Can be
  // registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_IOS_CHROME_TAB_RESTORE_SERVICE_FACTORY_H_
