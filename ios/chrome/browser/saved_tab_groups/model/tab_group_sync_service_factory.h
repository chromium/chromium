// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_SYNC_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace tab_groups {
class SyntheticFieldTrialHelper;
class TabGroupSyncService;

// Factory for the Tab Group Sync service.
class TabGroupSyncServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static TabGroupSyncService* GetForProfile(ProfileIOS* profile);
  static TabGroupSyncServiceFactory* GetInstance();

  // Return the default factory.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<TabGroupSyncServiceFactory>;

  TabGroupSyncServiceFactory();
  ~TabGroupSyncServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;

  // Called to report synthetic field trial on whether the client had a sync
  // tabgroup.
  static void OnHadSyncedTabGroup(bool had_synced_group);

  // Called to report synthetic field trial on whether the client had a shared
  // tabgroup.
  static void OnHadSharedTabGroup(bool had_shared_group);

  // Helper method to register a synthetic field trial.
  static void RegisterFieldTrial(std::string_view trial_name,
                                 std::string_view group_name);

  std::unique_ptr<SyntheticFieldTrialHelper> synthetic_field_trial_helper_;
};

}  // namespace tab_groups

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_SYNC_SERVICE_FACTORY_H_
