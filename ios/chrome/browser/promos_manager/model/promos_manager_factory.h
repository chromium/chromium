// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMOS_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMOS_MANAGER_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class PromosManager;

// Singleton that owns all PromosManagers and associates them with
// Profile.
class PromosManagerFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static PromosManager* GetForBrowserState(ProfileIOS* profile);

  static PromosManager* GetForProfile(ProfileIOS* profile);
  static PromosManagerFactory* GetInstance();

  PromosManagerFactory(const PromosManagerFactory&) = delete;
  PromosManagerFactory& operator=(const PromosManagerFactory&) = delete;

 private:
  friend class base::NoDestructor<PromosManagerFactory>;

  PromosManagerFactory();
  ~PromosManagerFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* state) const override;
};

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMOS_MANAGER_FACTORY_H_
