// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_SYNC_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_SYNC_SERVICE_FACTORY_H_

#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace tab_groups {
class SyntheticFieldTrialHelper;

// Factory for the Tab Group Sync service.
class TabGroupSyncServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<TabGroupSyncServiceFactory,
                                                TabGroupSyncService> {
 public:
  explicit TabGroupSyncServiceFactory(PassKey key);

  // Return the default factory.
  static TestingFactory GetDefaultFactory();

 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;

  // Called to report synthetic field trial on whether the client had a sync
  // tabgroup.
  static void OnHadSyncedTabGroup(bool had_synced_group);

  // Called to report synthetic field trial on whether the client had a shared
  // tabgroup.
  static void OnHadSharedTabGroup(bool had_shared_group);

  // Helper method to register a synthetic field trial.
  static void RegisterFieldTrial(std::string_view trial_name,
                                 std::string_view group_name);

  // Returns the singleton SyntheticFieldTrialHelper used by this instance.
  static SyntheticFieldTrialHelper* GetSyntheticFieldTrialHelper();
};

}  // namespace tab_groups

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_TAB_GROUP_SYNC_SERVICE_FACTORY_H_
