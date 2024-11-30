// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_SYNC_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace tab_groups {

class TabGroupSyncService;

// Factory for the Tab Group Sync service.
class TabGroupSyncServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static TabGroupSyncService* GetForProfile(ProfileIOS* profile);
  static TabGroupSyncServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<TabGroupSyncServiceFactory>;

  TabGroupSyncServiceFactory();
  ~TabGroupSyncServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace tab_groups

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_SYNC_SERVICE_FACTORY_H_
