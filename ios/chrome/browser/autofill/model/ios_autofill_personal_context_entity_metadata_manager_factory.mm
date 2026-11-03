// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/ios_autofill_personal_context_entity_metadata_manager_factory.h"

#import <memory>
#import <utility>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "components/autofill/core/browser/data_manager/autofill_ai/personal_context_entity_metadata_manager.h"
#import "components/autofill/core/browser/webdata/personal_context/personal_context_entity_metadata_sync_bridge.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/sync/base/data_type.h"
#import "components/sync/base/report_unrecoverable_error.h"
#import "components/sync/model/client_tag_based_data_type_processor.h"
#import "components/sync/model/data_type_store_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/chrome/common/channel_info.h"

IOSAutofillPersonalContextEntityMetadataManagerFactory::
    IOSAutofillPersonalContextEntityMetadataManagerFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "PersonalContextEntityMetadataManager",
          ProfileSelection::kNoInstanceInIncognito) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSAutofillPersonalContextEntityMetadataManagerFactory::BuildServiceInstanceFor(
    ProfileIOS& profile) const {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillAmbientAutofillSuppression)) {
    return nullptr;
  }

  auto change_processor =
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::AUTOFILL_ENTITY_SUPPRESSION,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              ::GetChannel()));
  auto sync_bridge =
      std::make_unique<autofill::PersonalContextEntityMetadataSyncBridge>(
          std::move(change_processor),
          DataTypeStoreServiceFactory::GetForProfile(profile)
              ->GetStoreFactory(),
          GetApplicationContext()->GetOSCryptAsync());
  return std::make_unique<autofill::PersonalContextEntityMetadataManager>(
      std::move(sync_bridge));
}
