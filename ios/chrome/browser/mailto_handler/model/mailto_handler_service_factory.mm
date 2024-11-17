// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mailto_handler/model/mailto_handler_service_factory.h"

#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/mailto_handler/model/mailto_handler_configuration.h"
#import "ios/chrome/browser/mailto_handler/model/mailto_handler_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/public/provider/chrome/browser/mailto_handler/mailto_handler_api.h"

// static
MailtoHandlerService* MailtoHandlerServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<MailtoHandlerService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
MailtoHandlerServiceFactory* MailtoHandlerServiceFactory::GetInstance() {
  static base::NoDestructor<MailtoHandlerServiceFactory> instance;
  return instance.get();
}

MailtoHandlerServiceFactory::MailtoHandlerServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "MailtoHandlerService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ChromeAccountManagerServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

MailtoHandlerServiceFactory::~MailtoHandlerServiceFactory() = default;

std::unique_ptr<KeyedService>
MailtoHandlerServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  MailtoHandlerConfiguration* config =
      [[MailtoHandlerConfiguration alloc] init];

  config.identityManager = IdentityManagerFactory::GetForProfile(profile);
  config.accountManager =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);

  ApplicationContext* application_context = GetApplicationContext();
  config.localState = application_context->GetLocalState();
  config.singleSignOnService = application_context->GetSingleSignOnService();

  return ios::provider::CreateMailtoHandlerService(config);
}

web::BrowserState* MailtoHandlerServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
