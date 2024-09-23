// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"

#include "base/no_destructor.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/password_manager/core/browser/affiliation/passkey_affiliation_source_adapter.h"
#include "components/sync/base/features.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/webauthn/core/browser/passkey_sync_bridge.h"
#include "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#include "ios/web/public/browser_state.h"

// static
webauthn::PasskeyModel* IOSPasskeyModelFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
webauthn::PasskeyModel* IOSPasskeyModelFactory::GetForProfile(
    ProfileIOS* profile) {
  if (!base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials)) {
    return nullptr;
  }
  return static_cast<webauthn::PasskeyModel*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
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
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(IOSChromeAffiliationServiceFactory::GetInstance());
}

IOSPasskeyModelFactory::~IOSPasskeyModelFactory() {}

std::unique_ptr<KeyedService> IOSPasskeyModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  auto sync_bridge = std::make_unique<webauthn::PasskeySyncBridge>(
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory());

  std::unique_ptr<password_manager::PasskeyAffiliationSourceAdapter> adapter =
      std::make_unique<password_manager::PasskeyAffiliationSourceAdapter>(
          sync_bridge.get());

  IOSChromeAffiliationServiceFactory::GetForBrowserState(profile)
      ->RegisterSource(std::move(adapter));
  return sync_bridge;
}
