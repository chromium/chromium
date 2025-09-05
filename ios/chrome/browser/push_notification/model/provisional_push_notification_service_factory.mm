// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/provisional_push_notification_service_factory.h"

#import "ios/chrome/browser/push_notification/model/provisional_push_notification_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"

// static
ProvisionalPushNotificationService*
ProvisionalPushNotificationServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<ProvisionalPushNotificationService>(
          profile, /*create=*/true);
}

// static
ProvisionalPushNotificationServiceFactory*
ProvisionalPushNotificationServiceFactory::GetInstance() {
  static base::NoDestructor<ProvisionalPushNotificationServiceFactory> instance;
  return instance.get();
}

ProvisionalPushNotificationServiceFactory::
    ProvisionalPushNotificationServiceFactory()
    : ProfileKeyedServiceFactoryIOS(
          "ProvisionalPushNotificationServiceFactory") {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
}

ProvisionalPushNotificationServiceFactory::
    ~ProvisionalPushNotificationServiceFactory() = default;

std::unique_ptr<KeyedService>
ProvisionalPushNotificationServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ProvisionalPushNotificationService>(
      IdentityManagerFactory::GetForProfile(profile),
      DeviceInfoSyncServiceFactory::GetForProfile(profile),
      GetApplicationContext()->GetPushNotificationService());
}
