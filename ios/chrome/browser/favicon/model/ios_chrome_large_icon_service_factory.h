// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_MODEL_IOS_CHROME_LARGE_ICON_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_FAVICON_MODEL_IOS_CHROME_LARGE_ICON_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class KeyedService;
class ProfileIOS;

namespace favicon {
class LargeIconService;
}

// Singleton that owns all LargeIconService and associates them with
// ProfileIOS.
class IOSChromeLargeIconServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static favicon::LargeIconService* GetForProfile(ProfileIOS* profile);

  static IOSChromeLargeIconServiceFactory* GetInstance();

  // Returns the default factory used to build LargeIconServices. Can be
  // registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<IOSChromeLargeIconServiceFactory>;

  IOSChromeLargeIconServiceFactory();
  ~IOSChromeLargeIconServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_FAVICON_MODEL_IOS_CHROME_LARGE_ICON_SERVICE_FACTORY_H_
