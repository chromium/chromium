// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"

#import "base/no_destructor.h"
#import "components/autofill/core/browser/country_type.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/strike_database/strike_database.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_util.h"
#import "ios/chrome/browser/autofill/model/strike_database_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"

// static
autofill::EntityDataManager* IOSAutofillEntityDataManagerFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<autofill::EntityDataManager>(
      profile, /*create=*/true);
}

// static
IOSAutofillEntityDataManagerFactory*
IOSAutofillEntityDataManagerFactory::GetInstance() {
  static base::NoDestructor<IOSAutofillEntityDataManagerFactory> instance;
  return instance.get();
}

IOSAutofillEntityDataManagerFactory::IOSAutofillEntityDataManagerFactory()
    : ProfileKeyedServiceFactoryIOS("EntityDataManager") {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(ios::WebDataServiceFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(autofill::StrikeDatabaseFactory::GetInstance());
}

IOSAutofillEntityDataManagerFactory::~IOSAutofillEntityDataManagerFactory() =
    default;

std::unique_ptr<KeyedService>
IOSAutofillEntityDataManagerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillAiCreateEntityDataManager)) {
    return nullptr;
  }

  return std::make_unique<autofill::EntityDataManager>(
      profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile),
      ios::WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      ios::HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      autofill::StrikeDatabaseFactory::GetForProfile(profile),
      /*accessibility_annotator_data_adapter=*/nullptr,
      autofill::GeoIpCountryCode(autofill::GetCountryCodeFromVariations()));
}
