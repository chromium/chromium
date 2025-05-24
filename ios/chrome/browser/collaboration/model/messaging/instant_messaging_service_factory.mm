// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/messaging/instant_messaging_service_factory.h"

#import <memory>

#import "base/check.h"
#import "components/collaboration/public/features.h"
#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"
#import "ios/chrome/browser/collaboration/model/features.h"
#import "ios/chrome/browser/collaboration/model/messaging/instant_messaging_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace collaboration::messaging {

// static
InstantMessagingService* InstantMessagingServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<InstantMessagingService>(
      profile, /*create=*/true);
}

// static
InstantMessagingServiceFactory* InstantMessagingServiceFactory::GetInstance() {
  static base::NoDestructor<InstantMessagingServiceFactory> instance;
  return instance.get();
}

InstantMessagingServiceFactory::InstantMessagingServiceFactory()
    : ProfileKeyedServiceFactoryIOS("InstantMessagingService") {
  DependsOn(collaboration::CollaborationServiceFactory::GetInstance());
}

InstantMessagingServiceFactory::~InstantMessagingServiceFactory() = default;

std::unique_ptr<KeyedService>
InstantMessagingServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  CHECK(!profile->IsOffTheRecord());

  collaboration::CollaborationService* collaboration_service =
      collaboration::CollaborationServiceFactory::GetForProfile(profile);
  if (!IsSharedTabGroupsJoinEnabled(collaboration_service) ||
      !base::FeatureList::IsEnabled(
          collaboration::features::kCollaborationMessaging)) {
    return nullptr;
  }

  return std::make_unique<InstantMessagingService>(profile);
}

}  // namespace collaboration::messaging
