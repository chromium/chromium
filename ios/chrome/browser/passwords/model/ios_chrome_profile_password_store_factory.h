// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PROFILE_PASSWORD_STORE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PROFILE_PASSWORD_STORE_FACTORY_H_

#import "base/memory/ref_counted.h"
#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/refcounted_profile_keyed_service_factory_ios.h"

enum class ServiceAccessType;

namespace password_manager {
class PasswordStoreInterface;
}

// Singleton that owns all PasswordStores and associates them with
// ProfileIOS.
class IOSChromeProfilePasswordStoreFactory
    : public RefcountedProfileKeyedServiceFactoryIOS {
 public:
  static scoped_refptr<password_manager::PasswordStoreInterface> GetForProfile(
      ProfileIOS* profile,
      ServiceAccessType access_type);
  static IOSChromeProfilePasswordStoreFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSChromeProfilePasswordStoreFactory>;

  IOSChromeProfilePasswordStoreFactory();
  ~IOSChromeProfilePasswordStoreFactory() override;

  // BrowserStateKeyedServiceFactory:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PROFILE_PASSWORD_STORE_FACTORY_H_
