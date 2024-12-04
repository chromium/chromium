// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class IOSChromeSafetyCheckManager;
class KeyedService;
class ProfileIOS;

// Singleton that owns all IOSChromeSafetyCheckManager(s) and associates them
// with profiles.
class IOSChromeSafetyCheckManagerFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static IOSChromeSafetyCheckManager* GetForProfile(ProfileIOS* profile);
  static IOSChromeSafetyCheckManagerFactory* GetInstance();
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<IOSChromeSafetyCheckManagerFactory>;

  IOSChromeSafetyCheckManagerFactory();
  ~IOSChromeSafetyCheckManagerFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SAFETY_CHECK_MODEL_IOS_CHROME_SAFETY_CHECK_MANAGER_FACTORY_H_
