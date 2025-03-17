// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_MODEL_INACTIVE_TABS_INACTIVE_TABS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_MODEL_INACTIVE_TABS_INACTIVE_TABS_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class InactiveTabsService;

// Singleton that creates the InactiveTabsService and associates that
// service with ProfileIOS.
class InactiveTabsServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static InactiveTabsServiceFactory* GetInstance();
  static InactiveTabsService* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<InactiveTabsServiceFactory>;

  InactiveTabsServiceFactory();
  ~InactiveTabsServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_MODEL_INACTIVE_TABS_INACTIVE_TABS_SERVICE_FACTORY_H_
