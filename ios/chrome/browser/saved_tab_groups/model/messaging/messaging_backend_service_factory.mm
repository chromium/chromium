// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/messaging/messaging_backend_service_factory.h"

#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/saved_tab_groups/messaging/messaging_backend_service_impl.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/data_sharing/model/features.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace tab_groups::messaging {

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
  DependsOn(TabGroupSyncServiceFactory::GetInstance());
  DependsOn(data_sharing::DataSharingServiceFactory::GetInstance());
}

MessagingBackendServiceFactory::~MessagingBackendServiceFactory() = default;

std::unique_ptr<KeyedService>
MessagingBackendServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = static_cast<ProfileIOS*>(context);
  CHECK(!profile->IsOffTheRecord());

  if (!IsSharedTabGroupsJoinEnabled(profile)) {
    return nullptr;
  }

  auto* tab_group_sync_service =
      TabGroupSyncServiceFactory::GetForProfile(profile);
  auto* data_sharing_service =
      data_sharing::DataSharingServiceFactory::GetForProfile(profile);

  return std::make_unique<MessagingBackendServiceImpl>(tab_group_sync_service,
                                                       data_sharing_service);
}

}  // namespace tab_groups::messaging
