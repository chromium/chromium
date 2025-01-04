// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/messaging/messaging_backend_service_factory.h"

#import <memory>

#import "components/collaboration/internal/messaging/configuration.h"
#import "components/collaboration/internal/messaging/data_sharing_change_notifier_impl.h"
#import "components/collaboration/internal/messaging/empty_messaging_backend_service.h"
#import "components/collaboration/internal/messaging/messaging_backend_service_impl.h"
#import "components/collaboration/internal/messaging/storage/messaging_backend_store_impl.h"
#import "components/collaboration/internal/messaging/tab_group_change_notifier_impl.h"
#import "components/collaboration/public/features.h"
#import "components/data_sharing/public/features.h"
#import "ios/chrome/browser/collaboration/model/features.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace collaboration::messaging {

// static
MessagingBackendService* MessagingBackendServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<MessagingBackendService>(
      profile, /*create=*/true);
}

// static
MessagingBackendServiceFactory* MessagingBackendServiceFactory::GetInstance() {
  static base::NoDestructor<MessagingBackendServiceFactory> instance;
  return instance.get();
}

MessagingBackendServiceFactory::MessagingBackendServiceFactory()
    : ProfileKeyedServiceFactoryIOS("MessagingBackendService",
                                    ProfileSelection::kNoInstanceInIncognito) {
  DependsOn(tab_groups::TabGroupSyncServiceFactory::GetInstance());
  DependsOn(data_sharing::DataSharingServiceFactory::GetInstance());
}

MessagingBackendServiceFactory::~MessagingBackendServiceFactory() = default;

std::unique_ptr<KeyedService>
MessagingBackendServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  CHECK(!profile->IsOffTheRecord());

  if (!base::FeatureList::IsEnabled(
          data_sharing::features::kDataSharingFeature) ||
      !IsSharedTabGroupsJoinEnabled(profile) ||
      !base::FeatureList::IsEnabled(
          collaboration::features::kCollaborationMessaging)) {
    return std::make_unique<EmptyMessagingBackendService>();
  }

  auto* tab_group_sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  auto* data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);
  auto tab_group_change_notifier =
      std::make_unique<TabGroupChangeNotifierImpl>(tab_group_sync_service);
  auto data_sharing_change_notifier =
      std::make_unique<DataSharingChangeNotifierImpl>(data_sharing_service);
  auto messaging_backend_store = std::make_unique<MessagingBackendStoreImpl>();

  // iOS does not need any specialized configuration.
  MessagingBackendConfiguration configuration;

  return std::make_unique<MessagingBackendServiceImpl>(
      configuration, std::move(tab_group_change_notifier),
      std::move(data_sharing_change_notifier),
      std::move(messaging_backend_store), tab_group_sync_service,
      data_sharing_service);
}

}  // namespace collaboration::messaging
