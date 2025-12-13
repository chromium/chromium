// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_STORE_BUNDLE_MODEL_APP_STORE_BUNDLE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_APP_STORE_BUNDLE_MODEL_APP_STORE_BUNDLE_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class AppStoreBundleService;
class ProfileIOS;

// Singleton that owns all AppStoreBundleService and associates them with
// a ProfileIOS.
class AppStoreBundleServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static AppStoreBundleService* GetForProfile(ProfileIOS* profile);
  static AppStoreBundleServiceFactory* GetInstance();

  AppStoreBundleServiceFactory(const AppStoreBundleServiceFactory&) = delete;
  AppStoreBundleServiceFactory& operator=(const AppStoreBundleServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<AppStoreBundleServiceFactory>;

  AppStoreBundleServiceFactory();
  ~AppStoreBundleServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_APP_STORE_BUNDLE_MODEL_APP_STORE_BUNDLE_SERVICE_FACTORY_H_
