// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class TabGroupService;

// Singleton that creates the TabGroupService and associates that
// service with ProfileIOS.
class TabGroupServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static TabGroupServiceFactory* GetInstance();
  static TabGroupService* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<TabGroupServiceFactory>;

  TabGroupServiceFactory();
  ~TabGroupServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_SERVICE_FACTORY_H_
