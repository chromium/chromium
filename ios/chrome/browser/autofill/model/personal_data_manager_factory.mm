// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"

#import <utility>

#import "base/feature_list.h"
#import "base/no_destructor.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/browser/strike_databases/strike_database.h"
#import "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/sync/base/command_line_switches.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/autofill/model/autofill_image_fetcher_factory.h"
#import "ios/chrome/browser/autofill/model/autofill_image_fetcher_impl.h"
#import "ios/chrome/browser/autofill/model/strike_database_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"

namespace autofill {

namespace {

// Return the latest country code from the chrome variation service.
// If the varaition service is not available, an empty string is returned.
const std::string GetCountryCodeFromVariations() {
  variations::VariationsService* variation_service =
      GetApplicationContext()->GetVariationsService();

  return variation_service
             ? base::ToUpperASCII(variation_service->GetLatestCountry())
             : std::string();
}
}  // namespace

// static
PersonalDataManager* PersonalDataManagerFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
PersonalDataManager* PersonalDataManagerFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<PersonalDataManager*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
PersonalDataManagerFactory* PersonalDataManagerFactory::GetInstance() {
  static base::NoDestructor<PersonalDataManagerFactory> instance;
  return instance.get();
}

PersonalDataManagerFactory::PersonalDataManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "PersonalDataManager",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ios::WebDataServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

PersonalDataManagerFactory::~PersonalDataManagerFactory() = default;

std::unique_ptr<KeyedService>
PersonalDataManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  scoped_refptr<autofill::AutofillWebDataService> local_storage =
      ios::WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
  scoped_refptr<autofill::AutofillWebDataService> account_storage =
      ios::WebDataServiceFactory::GetAutofillWebDataForAccount(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
  StrikeDatabase* strike_database =
      StrikeDatabaseFactory::GetForProfile(profile);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  AutofillImageFetcherBase* autofill_image_fetcher =
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableCardArtImage)
          ? AutofillImageFetcherFactory::GetForProfile(profile)
          : nullptr;

  return std::make_unique<PersonalDataManager>(
      local_storage, account_storage, profile->GetPrefs(),
      GetApplicationContext()->GetLocalState(),
      IdentityManagerFactory::GetForProfile(profile), history_service,
      sync_service, strike_database, autofill_image_fetcher,
      /*shared_storage_handler=*/nullptr,
      GetApplicationContext()->GetApplicationLocale(),
      GetCountryCodeFromVariations());
}

}  // namespace autofill
