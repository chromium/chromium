// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/provisional_push_notification_service_factory.h"

#import <utility>

#import "ios/chrome/browser/push_notification/model/provisional_push_notification_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"

ProvisionalPushNotificationServiceFactory::
    ProvisionalPushNotificationServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "ProvisionalPushNotificationServiceFactory") {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
ProvisionalPushNotificationServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ProvisionalPushNotificationService>(
      IdentityManagerFactory::GetForProfile(profile),
      DeviceInfoSyncServiceFactory::GetForProfile(profile),
      GetApplicationContext()->GetPushNotificationService());
}
