// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"

#import <algorithm>
#import <memory>
#import <string_view>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "components/collaboration/internal/collaboration_finder_impl.h"
#import "components/data_sharing/public/features.h"
#import "components/saved_tab_groups/public/synthetic_field_trial_helper.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "components/saved_tab_groups/public/tab_group_sync_service_factory_helper.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/data_sharing/model/personal_collaboration_data/personal_collaboration_data_service_factory.h"
#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_service_accessor.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_delegate.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_local_update_observer.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/common/channel_info.h"

namespace tab_groups {

namespace {
// Builds the service.
std::unique_ptr<KeyedService> BuildService(
    SyntheticFieldTrialHelper* synthetic_field_trial_helper,
    ProfileIOS* profile) {
  CHECK(!profile->IsOffTheRecord());

  // Give the opportunity for the test hook to override the factory from
  // the provider (allowing EG tests to use a fake TabGroupSyncService).
  if (auto tab_group_sync_service =
          tests_hook::CreateTabGroupSyncService(profile)) {
    return tab_group_sync_service;
  }

  syncer::DeviceInfoTracker* device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetDeviceInfoTracker();
  auto* opt_guide = OptimizationGuideServiceFactory::GetForProfile(profile);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  auto* personal_collaboration_data_service =
      data_sharing::personal_collaboration_data::
          PersonalCollaborationDataServiceFactory::GetForProfile(profile);
  auto* data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);
  auto collaboration_finder =
      std::make_unique<collaboration::CollaborationFinderImpl>(
          data_sharing_service);

  // TODO(crbug.com/439072431): Add support for iOS
  // PersonalCollaborationDataService.
  std::unique_ptr<TabGroupSyncService> tab_group_sync_service =
      CreateTabGroupSyncService(
          ::GetChannel(), DataTypeStoreServiceFactory::GetForProfile(profile),
          profile->GetPrefs(), device_info_tracker, opt_guide, identity_manager,
          personal_collaboration_data_service, std::move(collaboration_finder),
          synthetic_field_trial_helper, data_sharing_service->GetLogger());

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
  std::unique_ptr<TabGroupLocalUpdateObserver> local_update_observer =
      std::make_unique<TabGroupLocalUpdateObserver>(
          browser_list, tab_group_sync_service.get(),
          SessionRestorationServiceFactory::GetForProfile(profile));

  std::unique_ptr<IOSTabGroupSyncDelegate> delegate =
      std::make_unique<IOSTabGroupSyncDelegate>(
          browser_list, tab_group_sync_service.get(),
          std::move(local_update_observer));

  tab_group_sync_service->SetTabGroupSyncDelegate(std::move(delegate));
  return tab_group_sync_service;
}
}  // namespace

// static
TabGroupSyncService* TabGroupSyncServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<TabGroupSyncService>(
      profile, /*create=*/true);
}

// static
TabGroupSyncServiceFactory* TabGroupSyncServiceFactory::GetInstance() {
  static base::NoDestructor<TabGroupSyncServiceFactory> instance;
  return instance.get();
}

// static
TabGroupSyncServiceFactory::TestingFactory
TabGroupSyncServiceFactory::GetDefaultFactory() {
  // KeyedService factories are never destroyed, to base::Unretained(...)
  // is safe. See the implementation of GetInstance() for details.
  return base::BindOnce(
      &TabGroupSyncServiceFactory::BuildServiceInstanceFor,
      base::Unretained(TabGroupSyncServiceFactory::GetInstance()));
}

TabGroupSyncServiceFactory::TabGroupSyncServiceFactory()
    : ProfileKeyedServiceFactoryIOS("TabGroupSyncServiceFactory",
                                    TestingCreation::kNoServiceForTests,
                                    ServiceCreation::kCreateWithProfile),
      synthetic_field_trial_helper_(std::make_unique<SyntheticFieldTrialHelper>(
          base::BindRepeating(&TabGroupSyncServiceFactory::OnHadSyncedTabGroup),
          base::BindRepeating(
              &TabGroupSyncServiceFactory::OnHadSharedTabGroup))) {
  DependsOn(BrowserListFactory::GetInstance());
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(SessionRestorationServiceFactory::GetInstance());
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
  // The dependency on IdentityManager is only for the purpose of recording "on
  // signin" metrics.
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(data_sharing::DataSharingServiceFactory::GetInstance());
  DependsOn(data_sharing::personal_collaboration_data::
                PersonalCollaborationDataServiceFactory::GetInstance());
}

TabGroupSyncServiceFactory::~TabGroupSyncServiceFactory() = default;

std::unique_ptr<KeyedService>
TabGroupSyncServiceFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return BuildService(synthetic_field_trial_helper_.get(), profile);
}

// static
void TabGroupSyncServiceFactory::RegisterFieldTrial(
    std::string_view trial_name,
    std::string_view group_name) {
  IOSChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      trial_name, group_name,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}

// static
void TabGroupSyncServiceFactory::OnHadSyncedTabGroup(bool had_synced_group) {
  RegisterFieldTrial(kSyncedTabGroupFieldTrialName,
                     had_synced_group ? kHasOwnedTabGroupTypeName
                                      : kHasNotOwnedTabGroupTypeName);
}

// static
void TabGroupSyncServiceFactory::OnHadSharedTabGroup(bool had_shared_group) {
  RegisterFieldTrial(kSharedTabGroupFieldTrialName,
                     had_shared_group ? kHasOwnedTabGroupTypeName
                                      : kHasNotOwnedTabGroupTypeName);
}
}  // namespace tab_groups
