// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_IDENTITY_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_IDENTITY_MANAGER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace signin {
class IdentityManager;
}

// Singleton that owns all IdentityManager instances and associates them with
// Profiles.
class IdentityManagerFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the IdentityManager instance associated with `profile`.
  static signin::IdentityManager* GetForProfile(ProfileIOS* profile);

  // Returns an instance of the IdentityManagerFactory singleton.
  static IdentityManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<IdentityManagerFactory>;

  IdentityManagerFactory();
  ~IdentityManagerFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_IDENTITY_MANAGER_FACTORY_H_
