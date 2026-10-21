// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_MODEL_INACTIVE_TABS_INACTIVE_TABS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_MODEL_INACTIVE_TABS_INACTIVE_TABS_SERVICE_FACTORY_H_

#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"
#import "ios/chrome/browser/tab_switcher/model/inactive_tabs/inactive_tabs_service.h"

// Singleton that creates the InactiveTabsService and associates that
// service with ProfileIOS.
class InactiveTabsServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<InactiveTabsServiceFactory,
                                                InactiveTabsService> {
 public:
  explicit InactiveTabsServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_MODEL_INACTIVE_TABS_INACTIVE_TABS_SERVICE_FACTORY_H_
