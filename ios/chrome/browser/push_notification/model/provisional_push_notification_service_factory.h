// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PROVISIONAL_PUSH_NOTIFICATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PROVISIONAL_PUSH_NOTIFICATION_SERVICE_FACTORY_H_

#include "ios/chrome/browser/push_notification/model/provisional_push_notification_service.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Factory that owns all ProvisionalPushNotificationService instances and
// associates them to ProfileIOS instances.
class ProvisionalPushNotificationServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          ProvisionalPushNotificationServiceFactory,
          ProvisionalPushNotificationService> {
 public:
  explicit ProvisionalPushNotificationServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PROVISIONAL_PUSH_NOTIFICATION_SERVICE_FACTORY_H_
