// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/share_kit_service_factory.h"

#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"
#import "ios/chrome/browser/collaboration/model/features.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/saved_tab_groups/favicon/coordinator/tab_group_favicons_grid_configurator.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service_configuration.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/public/provider/chrome/browser/share_kit/share_kit_api.h"

// static
ShareKitService* ShareKitServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<ShareKitService*>(
      GetInstance()->GetServiceForProfileAs<ShareKitService>(profile,
                                                             /*create=*/true));
}

// static
ShareKitServiceFactory* ShareKitServiceFactory::GetInstance() {
  static base::NoDestructor<ShareKitServiceFactory> instance;
  return instance.get();
}

ShareKitServiceFactory::ShareKitServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ShareKitService",
                                    ProfileSelection::kNoInstanceInIncognito) {
  DependsOn(AuthenticationServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(data_sharing::DataSharingServiceFactory::GetInstance());
  DependsOn(collaboration::CollaborationServiceFactory::GetInstance());
  DependsOn(tab_groups::TabGroupSyncServiceFactory::GetInstance());
  DependsOn(IOSChromeFaviconLoaderFactory::GetInstance());
}

ShareKitServiceFactory::~ShareKitServiceFactory() = default;

std::unique_ptr<KeyedService> ShareKitServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = static_cast<ProfileIOS*>(context);

  if (!IsSharedTabGroupsJoinEnabled(profile) &&
      !IsSharedTabGroupsCreateEnabled(profile)) {
    return nullptr;
  }

  // Give the opportunity for the test hook to override the service from
  // the provider (allowing EG tests to use a test ShareKitService).
  if (auto share_kit_service = tests_hook::CreateShareKitService()) {
    return share_kit_service;
  }

  tab_groups::TabGroupSyncService* sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  FaviconLoader* favicon_loader =
      IOSChromeFaviconLoaderFactory::GetForProfile(profile);

  std::unique_ptr<ShareKitServiceConfiguration> configuration =
      std::make_unique<ShareKitServiceConfiguration>(
          IdentityManagerFactory::GetForProfile(profile),
          AuthenticationServiceFactory::GetForProfile(profile),
          data_sharing::DataSharingServiceFactory::GetForProfile(profile),
          sync_service,
          std::make_unique<TabGroupFaviconsGridConfigurator>(sync_service,
                                                             favicon_loader));
  return ios::provider::CreateShareKitService(std::move(configuration));
}
