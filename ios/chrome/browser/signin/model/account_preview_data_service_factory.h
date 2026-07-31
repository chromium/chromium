// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_PREVIEW_DATA_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_PREVIEW_DATA_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace signin {
class AccountPreviewDataService;
}

// KeyedService factory that creates the Profile-associated
// `AccountPreviewDataService`.
class AccountPreviewDataServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the AccountPreviewDataService instance associated with this
  // profile (creating one if it does not exist). Returns null if `profile`
  // is incognito or signin is disabled.
  static signin::AccountPreviewDataService* GetForProfile(ProfileIOS* profile);

  // Returns an instance of the factory singleton.
  static AccountPreviewDataServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<AccountPreviewDataServiceFactory>;

  AccountPreviewDataServiceFactory();
  ~AccountPreviewDataServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_ACCOUNT_PREVIEW_DATA_SERVICE_FACTORY_H_
