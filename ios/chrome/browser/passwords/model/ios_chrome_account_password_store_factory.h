// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_ACCOUNT_PASSWORD_STORE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_ACCOUNT_PASSWORD_STORE_FACTORY_H_

#import "base/memory/scoped_refptr.h"
#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/refcounted_profile_keyed_service_factory_ios.h"

enum class ServiceAccessType;

namespace password_manager {
class PasswordStoreInterface;
}

// Singleton that owns all Gaia-account-scoped PasswordStores and associates
// them with ProfileIOS.
class IOSChromeAccountPasswordStoreFactory
    : public RefcountedProfileKeyedServiceFactoryIOS {
 public:
  static scoped_refptr<password_manager::PasswordStoreInterface> GetForProfile(
      ProfileIOS* profile,
      ServiceAccessType access_type);
  static IOSChromeAccountPasswordStoreFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSChromeAccountPasswordStoreFactory>;

  IOSChromeAccountPasswordStoreFactory();
  ~IOSChromeAccountPasswordStoreFactory() override;

  // BrowserStateKeyedServiceFactory:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_ACCOUNT_PASSWORD_STORE_FACTORY_H_
