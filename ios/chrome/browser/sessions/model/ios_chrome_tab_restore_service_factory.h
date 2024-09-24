// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_IOS_CHROME_TAB_RESTORE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_IOS_CHROME_TAB_RESTORE_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace sessions {
class TabRestoreService;
}

// Singleton that owns all TabRestoreServices and associates them with
// ProfileIOS.
class IOSChromeTabRestoreServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static sessions::TabRestoreService* GetForBrowserState(ProfileIOS* profile);

  static sessions::TabRestoreService* GetForProfile(ProfileIOS* profile);
  static IOSChromeTabRestoreServiceFactory* GetInstance();

  // Returns the default factory used to build TabRestoreServices. Can be
  // registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

  IOSChromeTabRestoreServiceFactory(const IOSChromeTabRestoreServiceFactory&) =
      delete;
  IOSChromeTabRestoreServiceFactory& operator=(
      const IOSChromeTabRestoreServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSChromeTabRestoreServiceFactory>;

  IOSChromeTabRestoreServiceFactory();
  ~IOSChromeTabRestoreServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_IOS_CHROME_TAB_RESTORE_SERVICE_FACTORY_H_
