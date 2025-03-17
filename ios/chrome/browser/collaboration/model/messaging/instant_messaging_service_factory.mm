// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/messaging/instant_messaging_service_factory.h"

#import <memory>

#import "components/collaboration/public/features.h"
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
    : ProfileKeyedServiceFactoryIOS("InstantMessagingService",
                                    ProfileSelection::kNoInstanceInIncognito) {}

InstantMessagingServiceFactory::~InstantMessagingServiceFactory() = default;

std::unique_ptr<KeyedService>
InstantMessagingServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  CHECK(!profile->IsOffTheRecord());

  if (!IsSharedTabGroupsJoinEnabled(profile) ||
      !base::FeatureList::IsEnabled(
          collaboration::features::kCollaborationMessaging)) {
    return nullptr;
  }

  return std::make_unique<InstantMessagingService>();
}

}  // namespace collaboration::messaging
