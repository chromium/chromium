// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_SERVICE_FACTORY_H_

#import "ios/chrome/browser/content_notification/model/content_notification_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns ContentNotificationService and associates with
// profiles.
class ContentNotificationServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          ContentNotificationServiceFactory,
          ContentNotificationService> {
 public:
  explicit ContentNotificationServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_SERVICE_FACTORY_H_
