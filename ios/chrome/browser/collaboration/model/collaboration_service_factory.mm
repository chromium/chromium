// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"

#import <memory>

#import "components/collaboration/internal/collaboration_service_impl.h"
#import "components/collaboration/internal/empty_collaboration_service.h"
#import "components/data_sharing/public/features.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

namespace collaboration {

// static
CollaborationService* CollaborationServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<CollaborationService>(
      profile, /*create=*/true);
}

// static
CollaborationServiceFactory* CollaborationServiceFactory::GetInstance() {
  static base::NoDestructor<CollaborationServiceFactory> instance;
  return instance.get();
}

CollaborationServiceFactory::CollaborationServiceFactory()
    : ProfileKeyedServiceFactoryIOS("CollaborationService",
                                    ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(tab_groups::TabGroupSyncServiceFactory::GetInstance());
  DependsOn(data_sharing::DataSharingServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

CollaborationServiceFactory::~CollaborationServiceFactory() = default;

std::unique_ptr<KeyedService>
CollaborationServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  if (!context) {
    return nullptr;
  }

  ProfileIOS* profile = static_cast<ProfileIOS*>(context);

  bool isFeatureEnabled = base::FeatureList::IsEnabled(
                              data_sharing::features::kDataSharingFeature) ||
                          base::FeatureList::IsEnabled(
                              data_sharing::features::kDataSharingJoinOnly);

  if (!isFeatureEnabled || profile->IsOffTheRecord()) {
    return std::make_unique<EmptyCollaborationService>();
  }

  auto* tab_group_sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  auto* data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  auto* sync_service = SyncServiceFactory::GetForProfile(profile);

  return std::make_unique<CollaborationServiceImpl>(
      tab_group_sync_service, data_sharing_service, identity_manager,
      sync_service);
}

}  // namespace collaboration
