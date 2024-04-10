// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/password_manager/core/browser/affiliation/affiliations_prefetcher.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/webauthn/core/browser/passkey_sync_bridge.h"
#include "ios/chrome/browser/passwords/model/ios_chrome_affiliations_prefetcher_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sync/model/model_type_store_service_factory.h"
#include "ios/web/public/browser_state.h"

// static
webauthn::PasskeyModel* IOSPasskeyModelFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<webauthn::PasskeyModel*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
IOSPasskeyModelFactory* IOSPasskeyModelFactory::GetInstance() {
  static base::NoDestructor<IOSPasskeyModelFactory> instance;
  return instance.get();
}

IOSPasskeyModelFactory::IOSPasskeyModelFactory()
    : BrowserStateKeyedServiceFactory(
          "PasskeyModel",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
  DependsOn(IOSChromeAffiliationsPrefetcherFactory::GetInstance());
}

IOSPasskeyModelFactory::~IOSPasskeyModelFactory() {}

std::unique_ptr<KeyedService> IOSPasskeyModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  auto sync_bridge = std::make_unique<webauthn::PasskeySyncBridge>(
      ModelTypeStoreServiceFactory::GetForBrowserState(browser_state)
          ->GetStoreFactory());
  IOSChromeAffiliationsPrefetcherFactory::GetForBrowserState(browser_state)
      ->RegisterPasskeyModel(sync_bridge.get());
  return sync_bridge;
}
