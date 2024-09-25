// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_PROFILE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_PROFILE_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class PushNotificationProfileService;

// Singleton that creates the PushNotificationProfileService and associates that
// service with ProfileIOS.
class PushNotificationProfileServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static PushNotificationProfileServiceFactory* GetInstance();
  static PushNotificationProfileService* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<PushNotificationProfileServiceFactory>;

  PushNotificationProfileServiceFactory();
  ~PushNotificationProfileServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_PROFILE_SERVICE_FACTORY_H_
