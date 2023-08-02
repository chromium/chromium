// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/autofill/web_view_autocomplete_history_manager_factory.h"

#include <utility>

#include "base/no_destructor.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "ios/web_view/internal/webdata_services/web_view_web_data_service_wrapper_factory.h"

namespace ios_web_view {

// static
autofill::AutocompleteHistoryManager*
WebViewAutocompleteHistoryManagerFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<autofill::AutocompleteHistoryManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewAutocompleteHistoryManagerFactory*
WebViewAutocompleteHistoryManagerFactory::GetInstance() {
  static base::NoDestructor<WebViewAutocompleteHistoryManagerFactory> instance;
  return instance.get();
}

WebViewAutocompleteHistoryManagerFactory::
    WebViewAutocompleteHistoryManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "AutocompleteHistoryManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewWebDataServiceWrapperFactory::GetInstance());
}

WebViewAutocompleteHistoryManagerFactory::
    ~WebViewAutocompleteHistoryManagerFactory() {}

std::unique_ptr<KeyedService>
WebViewAutocompleteHistoryManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  std::unique_ptr<autofill::AutocompleteHistoryManager> service(
      new autofill::AutocompleteHistoryManager());
  auto profile_db =
      WebViewWebDataServiceWrapperFactory::GetAutofillWebDataForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  service->Init(profile_db, browser_state->GetPrefs(),
                browser_state->IsOffTheRecord());
  return service;
}

web::BrowserState*
WebViewAutocompleteHistoryManagerFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  return browser_state;
}

}  // namespace ios_web_view
