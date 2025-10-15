// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/web_view_personal_data_manager_factory.h"

#import <utility>

#import "base/no_destructor.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/autofill/cwv_autofill_prefs.h"
#import "ios/web_view/internal/autofill/web_view_autofill_image_fetcher_factory.h"
#import "ios/web_view/internal/autofill/web_view_autofill_image_fetcher_impl.h"
#import "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/sync/web_view_sync_service_factory.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/internal/webdata_services/web_view_web_data_service_wrapper_factory.h"

namespace ios_web_view {

// static
autofill::PersonalDataManager*
WebViewPersonalDataManagerFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<autofill::PersonalDataManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebViewPersonalDataManagerFactory*
WebViewPersonalDataManagerFactory::GetInstance() {
  static base::NoDestructor<WebViewPersonalDataManagerFactory> instance;
  return instance.get();
}

WebViewPersonalDataManagerFactory::WebViewPersonalDataManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "PersonalDataManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewIdentityManagerFactory::GetInstance());
  DependsOn(WebViewWebDataServiceWrapperFactory::GetInstance());
  DependsOn(WebViewSyncServiceFactory::GetInstance());
  DependsOn(WebViewAutofillImageFetcherFactory::GetInstance());
}

WebViewPersonalDataManagerFactory::~WebViewPersonalDataManagerFactory() {}

std::unique_ptr<KeyedService>
WebViewPersonalDataManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);
  auto profile_db =
      WebViewWebDataServiceWrapperFactory::GetAutofillWebDataForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  auto account_db =
      WebViewWebDataServiceWrapperFactory::GetAutofillWebDataForAccount(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  auto* sync_service =
      WebViewSyncServiceFactory::GetForBrowserState(browser_state);

  PrefService* prefs = browser_state->GetPrefs();
  autofill::AutofillImageFetcherBase* autofill_image_fetcher = nullptr;
  if (prefs->GetBoolean(ios_web_view::kUseImageFetcherEnabled)) {
    autofill_image_fetcher =
        WebViewAutofillImageFetcherFactory::GetForBrowserState(browser_state);
  }
  return std::make_unique<autofill::PersonalDataManager>(
      profile_db, account_db, browser_state->GetPrefs(),
      ApplicationContext::GetInstance()->GetLocalState(),
      WebViewIdentityManagerFactory::GetForBrowserState(browser_state),
      /*history_service=*/nullptr, sync_service, /*strike_database=*/nullptr,
      autofill_image_fetcher, /*shared_storage_handler=*/nullptr,
      ApplicationContext::GetInstance()->GetApplicationLocale(),
      /*country_code=*/"", /*autofill_optimization_guide=*/nullptr);
}

}  // namespace ios_web_view
