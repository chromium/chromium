// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class ContentNotificationService;

// Singleton that owns ContentNotificationService and associates with
// profiles.
class ContentNotificationServiceFactory final
    : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static ContentNotificationService* GetForBrowserState(ProfileIOS* profile);

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
