// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"

#include "base/no_destructor.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/password_manager/core/browser/affiliation/passkey_affiliation_source_adapter.h"
#include "components/sync/base/features.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/webauthn/core/browser/passkey_sync_bridge.h"
#include "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sync/model/model_type_store_service_factory.h"
#include "ios/web/public/browser_state.h"

// static
webauthn::PasskeyModel* IOSPasskeyModelFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials)
             ? static_cast<webauthn::PasskeyModel*>(
                   GetInstance()->GetServiceForBrowserState(browser_state,
                                                            true))
             : nullptr;
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
  DependsOn(IOSChromeAffiliationServiceFactory::GetInstance());
}

IOSPasskeyModelFactory::~IOSPasskeyModelFactory() {}

std::unique_ptr<KeyedService> IOSPasskeyModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  auto sync_bridge = std::make_unique<webauthn::PasskeySyncBridge>(
      ModelTypeStoreServiceFactory::GetForBrowserState(browser_state)
          ->GetStoreFactory());

  std::unique_ptr<password_manager::PasskeyAffiliationSourceAdapter> adapter =
      std::make_unique<password_manager::PasskeyAffiliationSourceAdapter>(
          sync_bridge.get());

  IOSChromeAffiliationServiceFactory::GetForBrowserState(browser_state)
      ->RegisterSource(std::move(adapter));
  return sync_bridge;
}
