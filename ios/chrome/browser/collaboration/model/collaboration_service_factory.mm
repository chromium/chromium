// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"

#import <memory>

#import "components/collaboration/internal/collaboration_service_impl.h"
#import "components/collaboration/internal/empty_collaboration_service.h"
#import "components/data_sharing/public/features.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

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
}

CollaborationServiceFactory::~CollaborationServiceFactory() = default;

std::unique_ptr<KeyedService>
CollaborationServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  if (!data_sharing::features::IsDataSharingFunctionalityEnabled() ||
      profile->IsOffTheRecord()) {
    return std::make_unique<EmptyCollaborationService>();
  }

  auto* tab_group_sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  auto* data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  auto* profile_prefs = profile->GetPrefs();

  return std::make_unique<CollaborationServiceImpl>(
      tab_group_sync_service, data_sharing_service, identity_manager,
      profile_prefs);
}

}  // namespace collaboration
