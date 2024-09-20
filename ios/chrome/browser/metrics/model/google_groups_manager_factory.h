// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_GOOGLE_GROUPS_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_GOOGLE_GROUPS_MANAGER_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class GoogleGroupsManager;

class GoogleGroupsManagerFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static GoogleGroupsManager* GetForBrowserState(ProfileIOS* profile);

  static GoogleGroupsManager* GetForProfile(ProfileIOS* profile);
  static GoogleGroupsManagerFactory* GetInstance();

  GoogleGroupsManagerFactory(const GoogleGroupsManagerFactory&) =
      delete;
  GoogleGroupsManagerFactory& operator=(
      const GoogleGroupsManagerFactory&) = delete;

 private:
  friend class base::NoDestructor<GoogleGroupsManagerFactory>;

  GoogleGroupsManagerFactory();
  ~GoogleGroupsManagerFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  bool ServiceIsCreatedWithBrowserState() const override;

  bool ServiceIsNULLWhileTesting() const override;

  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_GOOGLE_GROUPS_MANAGER_FACTORY_H_
