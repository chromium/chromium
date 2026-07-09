// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_TAB_CONTEXT_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_TAB_CONTEXT_SYNC_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace sync_tab_context {
class TabContextSyncService;
}  // namespace sync_tab_context

class TabContextSyncServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static sync_tab_context::TabContextSyncService* GetForProfile(
      ProfileIOS* profile);
  static TabContextSyncServiceFactory* GetInstance();

  TabContextSyncServiceFactory(const TabContextSyncServiceFactory&) = delete;
  TabContextSyncServiceFactory& operator=(const TabContextSyncServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<TabContextSyncServiceFactory>;

  TabContextSyncServiceFactory();
  ~TabContextSyncServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_TAB_CONTEXT_SYNC_SERVICE_FACTORY_H_
