// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/model/ios_at_memory_query_service_factory.h"

#import "base/feature_list.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/autofill/core/browser/at_memory/autofill_data_provider.h"
#import "components/autofill/core/browser/integrators/at_memory/at_memory_query_service.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/personal_context/core/personal_context_service.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/personal_context/model/ios_personal_context_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {
using autofill::AtMemoryQueryService;
using autofill::AutofillDataProvider;
using autofill::PersonalDataManagerFactory;
using personal_context::PersonalContextService;
}  // namespace

// static
AtMemoryQueryService* IOSAtMemoryQueryServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<AtMemoryQueryService>(
      profile, /*create=*/true);
}

// static
IOSAtMemoryQueryServiceFactory* IOSAtMemoryQueryServiceFactory::GetInstance() {
  static base::NoDestructor<IOSAtMemoryQueryServiceFactory> instance;
  return instance.get();
}

IOSAtMemoryQueryServiceFactory::IOSAtMemoryQueryServiceFactory()
    : ProfileKeyedServiceFactoryIOS("AtMemoryQueryService",
                                    ProfileSelection::kNoInstanceInIncognito) {
  DependsOn(PersonalDataManagerFactory::GetInstance());
  DependsOn(IOSAutofillEntityDataManagerFactory::GetInstance());
  DependsOn(IOSPersonalContextServiceFactory::GetInstance());
}

IOSAtMemoryQueryServiceFactory::~IOSAtMemoryQueryServiceFactory() = default;

std::unique_ptr<KeyedService>
IOSAtMemoryQueryServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(autofill::features::kAutofillAtMemory)) {
    return nullptr;
  }

  PersonalContextService* personal_context_service =
      IOSPersonalContextServiceFactory::GetForProfile(profile);
  if (!personal_context_service) {
    return nullptr;
  }

  std::unique_ptr<AutofillDataProvider> data_provider =
      std::make_unique<AutofillDataProvider>(
          PersonalDataManagerFactory::GetForProfile(profile),
          IOSAutofillEntityDataManagerFactory::GetForProfile(profile));

  return std::make_unique<AtMemoryQueryService>(
      std::move(data_provider), personal_context_service,
      GetApplicationContext()->GetApplicationLocaleStorage()->Get());
}
