// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/push_notification_browser_state_service_factory.h"

#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/commerce/price_alert_util.h"
#import "ios/chrome/browser/commerce/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/push_notification/push_notification_browser_state_service.h"
#import "ios/chrome/browser/push_notification/push_notification_service.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
PushNotificationBrowserStateServiceFactory*
PushNotificationBrowserStateServiceFactory::GetInstance() {
  static base::NoDestructor<PushNotificationBrowserStateServiceFactory>
      instance;
  return instance.get();
}

// static
PushNotificationBrowserStateService*
PushNotificationBrowserStateServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  if (!IsPriceNotificationsEnabled()) {
    return nullptr;
  }

  return static_cast<PushNotificationBrowserStateService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

PushNotificationBrowserStateServiceFactory::
    PushNotificationBrowserStateServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "PushNotificationBrowserStateService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

PushNotificationBrowserStateServiceFactory::
    ~PushNotificationBrowserStateServiceFactory() {}

std::unique_ptr<KeyedService>
PushNotificationBrowserStateServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);

  return std::make_unique<PushNotificationBrowserStateService>(
      identity_manager, browser_state->GetStatePath());
}

bool PushNotificationBrowserStateServiceFactory::ServiceIsNULLWhileTesting()
    const {
  return true;
}
