// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/autocomplete_history_manager_factory.h"

#import <utility>

#import "base/no_destructor.h"
#import "components/autofill/core/browser/single_field_fillers/autocomplete/autocomplete_history_manager.h"
#import "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"

namespace autofill {

// static
AutocompleteHistoryManager* AutocompleteHistoryManagerFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<AutocompleteHistoryManager>(
      profile, /*create=*/true);
}

// static
AutocompleteHistoryManagerFactory*
AutocompleteHistoryManagerFactory::GetInstance() {
  static base::NoDestructor<AutocompleteHistoryManagerFactory> instance;
  return instance.get();
}

AutocompleteHistoryManagerFactory::AutocompleteHistoryManagerFactory()
    : ProfileKeyedServiceFactoryIOS("AutocompleteHistoryManager",
                                    ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(ios::WebDataServiceFactory::GetInstance());
}

AutocompleteHistoryManagerFactory::~AutocompleteHistoryManagerFactory() {}

std::unique_ptr<KeyedService>
AutocompleteHistoryManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  auto service = std::make_unique<AutocompleteHistoryManager>();
  scoped_refptr<autofill::AutofillWebDataService> autofill_db =
      ios::WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
  service->Init(autofill_db, profile->GetPrefs(), profile->IsOffTheRecord());
  return service;
}

}  // namespace autofill
