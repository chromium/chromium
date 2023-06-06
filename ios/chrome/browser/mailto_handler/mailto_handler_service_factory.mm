// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mailto_handler/mailto_handler_service_factory.h"

#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/mailto_handler/mailto_handler_configuration.h"
#import "ios/chrome/browser/mailto_handler/mailto_handler_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/public/provider/chrome/browser/mailto_handler/mailto_handler_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
MailtoHandlerService* MailtoHandlerServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<MailtoHandlerService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
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
  DependsOn(AuthenticationServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(SyncSetupServiceFactory::GetInstance());
}

MailtoHandlerServiceFactory::~MailtoHandlerServiceFactory() = default;

std::unique_ptr<KeyedService>
MailtoHandlerServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  MailtoHandlerConfiguration* configuration =
      [[MailtoHandlerConfiguration alloc] init];

  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  configuration.authService =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  configuration.syncService =
      SyncServiceFactory::GetForBrowserState(browser_state);
  configuration.syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(browser_state);

  ApplicationContext* application_context = GetApplicationContext();
  configuration.localState = application_context->GetLocalState();
  configuration.ssoService = application_context->GetSSOService();

  return ios::provider::CreateMailtoHandlerService(configuration);
}

web::BrowserState* MailtoHandlerServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
