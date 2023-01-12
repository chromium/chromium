// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/consent_auditor/consent_auditor_factory.h"

#import <memory>
#import <utility>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/ptr_util.h"
#import "base/no_destructor.h"
#import "base/time/default_clock.h"
#import "components/consent_auditor/consent_auditor_impl.h"
#import "components/consent_auditor/consent_sync_bridge.h"
#import "components/consent_auditor/consent_sync_bridge_impl.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/sync/base/report_unrecoverable_error.h"
#import "components/sync/model/client_tag_based_model_type_processor.h"
#import "components/sync/model/model_type_store_service.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/sync/model_type_store_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
consent_auditor::ConsentAuditor* ConsentAuditorFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<consent_auditor::ConsentAuditor*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
consent_auditor::ConsentAuditor*
ConsentAuditorFactory::GetForBrowserStateIfExists(
    ChromeBrowserState* browser_state) {
  return static_cast<consent_auditor::ConsentAuditor*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
ConsentAuditorFactory* ConsentAuditorFactory::GetInstance() {
  static base::NoDestructor<ConsentAuditorFactory> instance;
  return instance.get();
}

ConsentAuditorFactory::ConsentAuditorFactory()
    : BrowserStateKeyedServiceFactory(
          "ConsentAuditor",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
}

ConsentAuditorFactory::~ConsentAuditorFactory() {}

std::unique_ptr<KeyedService> ConsentAuditorFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  ChromeBrowserState* ios_browser_state =
      ChromeBrowserState::FromBrowserState(browser_state);

  std::unique_ptr<consent_auditor::ConsentSyncBridge> consent_sync_bridge;
  syncer::OnceModelTypeStoreFactory store_factory =
      ModelTypeStoreServiceFactory::GetForBrowserState(ios_browser_state)
          ->GetStoreFactory();
  auto change_processor =
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::USER_CONSENTS,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              ::GetChannel()));
  consent_sync_bridge =
      std::make_unique<consent_auditor::ConsentSyncBridgeImpl>(
          std::move(store_factory), std::move(change_processor));

  return std::make_unique<consent_auditor::ConsentAuditorImpl>(
      std::move(consent_sync_bridge),
      // The locale doesn't change at runtime, so we can pass it directly.
      GetApplicationContext()->GetApplicationLocale(),
      base::DefaultClock::GetInstance());
}
