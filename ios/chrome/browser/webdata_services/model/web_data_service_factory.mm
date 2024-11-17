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
#import "components/plus_addresses/webdata/plus_address_webdata_service.h"
#import "components/search_engines/keyword_web_data_service.h"
#import "components/signin/public/webdata/token_web_data.h"
#import "components/webdata_services/web_data_service_wrapper.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

namespace ios {

namespace {

std::unique_ptr<KeyedService> BuildWebDataService(web::BrowserState* context) {
  const base::FilePath& state_path = context->GetStatePath();
  // On iOS (and Android), the account storage is persisted on disk.
  return std::make_unique<WebDataServiceWrapper>(
      state_path, GetApplicationContext()->GetApplicationLocale(),
      web::GetUIThreadTaskRunner({}), base::DoNothing(),
      GetApplicationContext()->GetOSCryptAsync(),
      /*use_in_memory_autofill_account_database=*/false);
}

}  // namespace

// static
WebDataServiceWrapper* WebDataServiceFactory::GetForProfile(
    ProfileIOS* profile,
    ServiceAccessType access_type) {
  DCHECK(access_type == ServiceAccessType::EXPLICIT_ACCESS ||
         !profile->IsOffTheRecord());
  return static_cast<WebDataServiceWrapper*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
WebDataServiceWrapper* WebDataServiceFactory::GetForProfileIfExists(
    ProfileIOS* profile,
    ServiceAccessType access_type) {
  DCHECK(access_type == ServiceAccessType::EXPLICIT_ACCESS ||
         !profile->IsOffTheRecord());
  return static_cast<WebDataServiceWrapper*>(
      GetInstance()->GetServiceForBrowserState(profile, false));
}

// static
scoped_refptr<autofill::AutofillWebDataService>
WebDataServiceFactory::GetAutofillWebDataForProfile(
    ProfileIOS* profile,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper = GetForProfile(profile, access_type);
  return wrapper ? wrapper->GetProfileAutofillWebData() : nullptr;
}

// static
scoped_refptr<autofill::AutofillWebDataService>
WebDataServiceFactory::GetAutofillWebDataForAccount(
    ProfileIOS* profile,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper = GetForProfile(profile, access_type);
  return wrapper ? wrapper->GetAccountAutofillWebData() : nullptr;
}

// static
scoped_refptr<KeywordWebDataService>
WebDataServiceFactory::GetKeywordWebDataForProfile(
    ProfileIOS* profile,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper = GetForProfile(profile, access_type);
  return wrapper ? wrapper->GetKeywordWebData() : nullptr;
}

// static
scoped_refptr<plus_addresses::PlusAddressWebDataService>
WebDataServiceFactory::GetPlusAddressWebDataForProfile(
    ProfileIOS* profile,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper = GetForProfile(profile, access_type);
  return wrapper ? wrapper->GetPlusAddressWebData() : nullptr;
}

// static
scoped_refptr<TokenWebData> WebDataServiceFactory::GetTokenWebDataForProfile(
    ProfileIOS* profile,
    ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper = GetForProfile(profile, access_type);
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
