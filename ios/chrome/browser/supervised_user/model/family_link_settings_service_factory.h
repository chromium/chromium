// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_FAMILY_LINK_SETTINGS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_FAMILY_LINK_SETTINGS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace supervised_user {
class FamilyLinkSettingsService;

// Singleton that owns SupervisedUserSettingsService object and associates
// them with ProfileIOS.
class FamilyLinkSettingsServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static supervised_user::FamilyLinkSettingsService* GetForProfile(
      ProfileIOS* profile);
  static FamilyLinkSettingsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<FamilyLinkSettingsServiceFactory>;

  FamilyLinkSettingsServiceFactory();
  ~FamilyLinkSettingsServiceFactory() override = default;

  // ProfileKeyedServiceFactoryIOS implementation.
  bool ServiceIsRequiredForContextInitialization() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};
}  // namespace supervised_user

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_FAMILY_LINK_SETTINGS_SERVICE_FACTORY_H_
