// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PROFILE_PASSWORD_STORE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PROFILE_PASSWORD_STORE_FACTORY_H_

#import "base/memory/ref_counted.h"
#import "base/no_destructor.h"
#import "components/keyed_service/ios/refcounted_browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

enum class ServiceAccessType;

namespace password_manager {
class PasswordStoreInterface;
}

// Singleton that owns all PasswordStores and associates them with
// ProfileIOS.
class IOSChromeProfilePasswordStoreFactory
    : public RefcountedBrowserStateKeyedServiceFactory {
 public:
  static scoped_refptr<password_manager::PasswordStoreInterface>
  GetForBrowserState(ProfileIOS* profile, ServiceAccessType access_type);

  static scoped_refptr<password_manager::PasswordStoreInterface> GetForProfile(
      ProfileIOS* profile,
      ServiceAccessType access_type);
  static IOSChromeProfilePasswordStoreFactory* GetInstance();

  IOSChromeProfilePasswordStoreFactory(
      const IOSChromeProfilePasswordStoreFactory&) = delete;
  IOSChromeProfilePasswordStoreFactory& operator=(
      const IOSChromeProfilePasswordStoreFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSChromeProfilePasswordStoreFactory>;

  IOSChromeProfilePasswordStoreFactory();
  ~IOSChromeProfilePasswordStoreFactory() override;

  // BrowserStateKeyedServiceFactory:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_PROFILE_PASSWORD_STORE_FACTORY_H_
