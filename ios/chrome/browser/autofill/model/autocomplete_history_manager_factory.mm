// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autocomplete_history_manager_factory.h"

#import <utility>

#import "base/no_destructor.h"
#import "components/autofill/core/browser/autocomplete_history_manager.h"
#import "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"

namespace autofill {

// static
AutocompleteHistoryManager*
AutocompleteHistoryManagerFactory::GetForBrowserState(ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
AutocompleteHistoryManager* AutocompleteHistoryManagerFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<AutocompleteHistoryManager*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
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
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  std::unique_ptr<AutocompleteHistoryManager> service(
      new AutocompleteHistoryManager());
  scoped_refptr<autofill::AutofillWebDataService> autofill_db =
      ios::WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
  service->Init(autofill_db, profile->GetPrefs(), profile->IsOffTheRecord());
  return service;
}

web::BrowserState* AutocompleteHistoryManagerFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}

}  // namespace autofill
