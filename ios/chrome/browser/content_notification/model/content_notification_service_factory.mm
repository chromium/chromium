// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_notification/model/content_notification_service_factory.h"

#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/content_notification/model/content_notification_service.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/public/provider/chrome/browser/content_notification/content_notification_api.h"

// static
ContentNotificationService*
ContentNotificationServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<ContentNotificationService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
ContentNotificationServiceFactory*
ContentNotificationServiceFactory::GetInstance() {
  static base::NoDestructor<ContentNotificationServiceFactory> instance;
  return instance.get();
}

ContentNotificationServiceFactory::ContentNotificationServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "ContentNotificationService",
          BrowserStateDependencyManager::GetInstance()) {}

ContentNotificationServiceFactory::~ContentNotificationServiceFactory() =
    default;

std::unique_ptr<KeyedService>
ContentNotificationServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return ios::provider::CreateContentNotificationService();
}
