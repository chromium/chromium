// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/web_view_autocomplete_history_manager_factory.h"

#import <utility>

#import "base/no_destructor.h"
#import "components/autofill/core/browser/single_field_fillers/autocomplete/autocomplete_history_manager.h"
#import "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/internal/webdata_services/web_view_web_data_service_wrapper_factory.h"

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
  auto profile_db =
      WebViewWebDataServiceWrapperFactory::GetAutofillWebDataForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  return std::make_unique<autofill::AutocompleteHistoryManager>(
      profile_db, browser_state->GetPrefs());
}

}  // namespace ios_web_view
