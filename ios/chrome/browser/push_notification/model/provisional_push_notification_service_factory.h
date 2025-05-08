// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PROVISIONAL_PUSH_NOTIFICATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PROVISIONAL_PUSH_NOTIFICATION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class ProvisionalPushNotificationService;

// Factory that owns all ProvisionalPushNotificationService instances and
// associates them to ProfileIOS instances.
class ProvisionalPushNotificationServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static ProvisionalPushNotificationService* GetForProfile(ProfileIOS* profile);
  static ProvisionalPushNotificationServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ProvisionalPushNotificationServiceFactory>;

  ProvisionalPushNotificationServiceFactory();
  ~ProvisionalPushNotificationServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PROVISIONAL_PUSH_NOTIFICATION_SERVICE_FACTORY_H_
