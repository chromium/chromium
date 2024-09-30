// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_IDLE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_IDLE_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_service.h"

namespace enterprise_idle {

// Singleton that owns all IdleServices and associates them with
// ProfileIOS.
class IdleServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  IdleServiceFactory(const BrowserStateKeyedServiceFactory&) = delete;
  IdleServiceFactory& operator=(const BrowserStateKeyedServiceFactory&) =
      delete;

  // TODO(crbug.com/358301380): remove this method.
  static IdleService* GetForBrowserState(ChromeBrowserState* browser_state);

  static IdleService* GetForProfile(ProfileIOS* profile);
  static IdleServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IdleServiceFactory>;

  IdleServiceFactory();
  ~IdleServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace enterprise_idle

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_IDLE_SERVICE_FACTORY_H_
