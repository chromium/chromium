// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_PROFILE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_PROFILE_SERVICE_FACTORY_H_

#import "ios/chrome/browser/push_notification/model/push_notification_profile_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that creates the PushNotificationProfileService and associates that
// service with ProfileIOS.
class PushNotificationProfileServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          PushNotificationProfileServiceFactory,
          PushNotificationProfileService> {
 public:
  explicit PushNotificationProfileServiceFactory(PassKey key);

  // Returns the default factory used to build PushNotificationProfileService.
  // Can be registered with AddTestingFactory to use real instances during
  // testing.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_PROFILE_SERVICE_FACTORY_H_
