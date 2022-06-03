// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autofill/autocomplete_history_manager_factory.h"

#include <utility>

#include "base/no_destructor.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/autofill_profile_validator_factory.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/webdata_services/web_data_service_factory.h"

namespace autofill {

// static
AutocompleteHistoryManager*
AutocompleteHistoryManagerFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<AutocompleteHistoryManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
AutocompleteHistoryManagerFactory*
AutocompleteHistoryManagerFactory::GetInstance() {
  static base::NoDestructor<AutocompleteHistoryManagerFactory> instance;
  return instance.get();
}

AutocompleteHistoryManagerFactory::AutocompleteHistoryManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "AutocompleteHistoryManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::WebDataServiceFactory::GetInstance());
}

AutocompleteHistoryManagerFactory::~AutocompleteHistoryManagerFactory() {}

std::unique_ptr<KeyedService>
AutocompleteHistoryManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(context);
  std::unique_ptr<AutocompleteHistoryManager> service(
      new AutocompleteHistoryManager());
  auto autofill_db =
      ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
          chrome_browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  service->Init(autofill_db, chrome_browser_state->GetPrefs(),
                chrome_browser_state->IsOffTheRecord());
  return service;
}

web::BrowserState* AutocompleteHistoryManagerFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}

}  // namespace autofill
