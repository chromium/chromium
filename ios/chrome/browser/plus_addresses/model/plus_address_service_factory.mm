// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/plus_addresses/model/plus_address_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/plus_addresses/features.h"
#import "components/plus_addresses/plus_address_client.h"
#import "components/plus_addresses/plus_address_service.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"

// static
plus_addresses::PlusAddressService*
PlusAddressServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<plus_addresses::PlusAddressService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
PlusAddressServiceFactory* PlusAddressServiceFactory::GetInstance() {
  static base::NoDestructor<PlusAddressServiceFactory> instance;
  return instance.get();
}

PlusAddressServiceFactory::PlusAddressServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "PlusAddressService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

PlusAddressServiceFactory::~PlusAddressServiceFactory() {}

std::unique_ptr<KeyedService>
PlusAddressServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  // If the feature is disabled, don't risk any side effects. Just bail.
  if (!base::FeatureList::IsEnabled(plus_addresses::kFeature)) {
    return nullptr;
  }

  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  return std::make_unique<plus_addresses::PlusAddressService>(
      identity_manager, browser_state->GetPrefs(),
      plus_addresses::PlusAddressClient(
          identity_manager, browser_state->GetSharedURLLoaderFactory()));
}

bool PlusAddressServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
