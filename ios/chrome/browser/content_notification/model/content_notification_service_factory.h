// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ContentNotificationService;
class ProfileIOS;

// Singleton that owns ContentNotificationService and associates with
// profiles.
class ContentNotificationServiceFactory final
    : public BrowserStateKeyedServiceFactory {
 public:
  static ContentNotificationService* GetForProfile(ProfileIOS* profile);
  static ContentNotificationServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ContentNotificationServiceFactory>;

  ContentNotificationServiceFactory();
  ~ContentNotificationServiceFactory() final;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const final;
};

#endif  // IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_SERVICE_FACTORY_H_
