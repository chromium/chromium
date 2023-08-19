// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autocomplete_history_manager_factory.h"

#import <utility>

#import "base/no_destructor.h"
#import "components/autofill/core/browser/autocomplete_history_manager.h"
#import "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/webdata_services/web_data_service_factory.h"

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
