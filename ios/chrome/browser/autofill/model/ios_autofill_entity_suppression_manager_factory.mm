// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/ios_autofill_entity_suppression_manager_factory.h"

#import <memory>
#import <utility>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_manager.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_manager_impl.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_sync_bridge.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/sync/base/data_type.h"
#import "components/sync/base/report_unrecoverable_error.h"
#import "components/sync/model/client_tag_based_data_type_processor.h"
#import "components/sync/model/data_type_store_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/chrome/common/channel_info.h"

// static
autofill::EntitySuppressionManager*
IOSAutofillEntitySuppressionManagerFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<autofill::EntitySuppressionManager>(
          profile, /*create=*/true);
}

// static
IOSAutofillEntitySuppressionManagerFactory*
IOSAutofillEntitySuppressionManagerFactory::GetInstance() {
  static base::NoDestructor<IOSAutofillEntitySuppressionManagerFactory>
      instance;
  return instance.get();
}

IOSAutofillEntitySuppressionManagerFactory::
    IOSAutofillEntitySuppressionManagerFactory()
    : ProfileKeyedServiceFactoryIOS("EntitySuppressionManager",
                                    ProfileSelection::kNoInstanceInIncognito) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

IOSAutofillEntitySuppressionManagerFactory::
    ~IOSAutofillEntitySuppressionManagerFactory() = default;

std::unique_ptr<KeyedService>
IOSAutofillEntitySuppressionManagerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillAmbientAutofillSuppression)) {
    return nullptr;
  }

  auto change_processor =
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::AUTOFILL_ENTITY_SUPPRESSION,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              ::GetChannel()));
  auto sync_bridge = std::make_unique<autofill::EntitySuppressionSyncBridge>(
      std::move(change_processor),
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory(),
      GetApplicationContext()->GetOSCryptAsync());
  return std::make_unique<autofill::EntitySuppressionManagerImpl>(
      std::move(sync_bridge));
}
