// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_PROFILE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_PROFILE_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class PushNotificationBrowserStateService;

// Singleton that creates the PushNotificationProfileService and associates that
// service with ChromeBrowserState.
// TODO(crbug.com/361040570): Rename this class to
// PushNotificationProfileServiceFactory.
class PushNotificationBrowserStateServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static PushNotificationBrowserStateServiceFactory* GetInstance();
  static PushNotificationBrowserStateService* GetForBrowserState(
      ChromeBrowserState* browser_state);

 private:
  friend class base::NoDestructor<PushNotificationBrowserStateServiceFactory>;

  PushNotificationBrowserStateServiceFactory();
  ~PushNotificationBrowserStateServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_PROFILE_SERVICE_FACTORY_H_
