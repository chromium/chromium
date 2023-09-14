// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"

#import "base/check.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/no_destructor.h"
#import "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/search_engines/keyword_web_data_service.h"
#import "components/signin/public/webdata/token_web_data.h"
#import "components/webdata_services/web_data_service_wrapper.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

namespace ios {

namespace {

std::unique_ptr<KeyedService> BuildWebDataService(web::BrowserState* context) {
  const base::FilePath& browser_state_path = context->GetStatePath();
  return std::make_unique<WebDataServiceWrapper>(
      browser_state_path, GetApplicationContext()->GetApplicationLocale(),
      web::GetUIThreadTaskRunner({}), base::DoNothing());
}

}  // namespace

// static
WebDataServiceWrapper* WebDataServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state,
    ServiceAccessType access_type) {
  DCHECK(access_type == ServiceAccessType::EXPLICIT_ACCESS ||
         !browser_state->IsOffTheRecord());
  return static_cast<WebDataServiceWrapper*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
WebDataServiceWrapper* WebDataServiceFactory::GetForBrowserStateIfExists(
    ChromeBrowserState* browser_state,
    ServiceAccessType access_type) {
  DCHECK(access_type == ServiceAccessType::EXPLICIT_ACCESS ||
         !browser_state->IsOffTheRecord());
  return static_cast<WebDataServiceWrapper*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
scoped_refptr<autofill::AutofillWebDataService>
WebDataServiceFactory::GetAutofillWebDataForBrowserState(
    ChromeBrowserState* browser_state,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      GetForBrowserState(browser_state, access_type);
  return wrapper ? wrapper->GetProfileAutofillWebData() : nullptr;
}

// static
scoped_refptr<autofill::AutofillWebDataService>
WebDataServiceFactory::GetAutofillWebDataForAccount(
    ChromeBrowserState* browser_state,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      GetForBrowserState(browser_state, access_type);
  return wrapper ? wrapper->GetAccountAutofillWebData() : nullptr;
}

// static
scoped_refptr<KeywordWebDataService>
WebDataServiceFactory::GetKeywordWebDataForBrowserState(
    ChromeBrowserState* browser_state,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      GetForBrowserState(browser_state, access_type);
  return wrapper ? wrapper->GetKeywordWebData() : nullptr;
}

// static
scoped_refptr<TokenWebData>
WebDataServiceFactory::GetTokenWebDataForBrowserState(
    ChromeBrowserState* browser_state,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      GetForBrowserState(browser_state, access_type);
  return wrapper ? wrapper->GetTokenWebData() : nullptr;
}

// static
WebDataServiceFactory* WebDataServiceFactory::GetInstance() {
  static base::NoDestructor<WebDataServiceFactory> instance;
  return instance.get();
}

// static
BrowserStateKeyedServiceFactory::TestingFactory
WebDataServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildWebDataService);
}

WebDataServiceFactory::WebDataServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "WebDataService",
          BrowserStateDependencyManager::GetInstance()) {}

WebDataServiceFactory::~WebDataServiceFactory() {}

std::unique_ptr<KeyedService> WebDataServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildWebDataService(context);
}

web::BrowserState* WebDataServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool WebDataServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios
