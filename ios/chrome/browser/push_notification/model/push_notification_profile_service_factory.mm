// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_profile_service_factory.h"

#import "base/memory/scoped_refptr.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/commerce/model/price_alert_util.h"
#import "ios/chrome/browser/commerce/model/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_manager.h"
#import "ios/chrome/browser/push_notification/model/push_notification_profile_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

// static
PushNotificationProfileServiceFactory*
PushNotificationProfileServiceFactory::GetInstance() {
  static base::NoDestructor<PushNotificationProfileServiceFactory> instance;
  return instance.get();
}

// static
PushNotificationProfileService*
PushNotificationProfileServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<PushNotificationProfileService>(
      profile, /*create=*/true);
}

PushNotificationProfileServiceFactory::PushNotificationProfileServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PushNotificationProfileService",
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

PushNotificationProfileServiceFactory::
    ~PushNotificationProfileServiceFactory() = default;

std::unique_ptr<KeyedService>
PushNotificationProfileServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  const scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();

  std::unique_ptr<PushNotificationClientManager> client_manager =
      IsMultiProfilePushNotificationHandlingEnabled()
          ? std::make_unique<PushNotificationClientManager>(task_runner,
                                                            profile)
          : nullptr;

  return std::make_unique<PushNotificationProfileService>(
      IdentityManagerFactory::GetForProfile(profile), std::move(client_manager),
      profile->GetStatePath());
}
