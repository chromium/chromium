// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_notification/model/content_notification_service_factory.h"

#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/content_notification/model/content_notification_configuration.h"
#import "ios/chrome/browser/content_notification/model/content_notification_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/public/provider/chrome/browser/content_notification/content_notification_api.h"

// static
ContentNotificationService* ContentNotificationServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<ContentNotificationService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
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
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ChromeAccountManagerServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

ContentNotificationServiceFactory::~ContentNotificationServiceFactory() =
    default;

std::unique_ptr<KeyedService>
ContentNotificationServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  ContentNotificationConfiguration* config =
      [[ContentNotificationConfiguration alloc] init];

  config.identityManager = IdentityManagerFactory::GetForProfile(profile);
  config.accountManager =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);

  config.ssoService = GetApplicationContext()->GetSingleSignOnService();

  return ios::provider::CreateContentNotificationService(config);
}
