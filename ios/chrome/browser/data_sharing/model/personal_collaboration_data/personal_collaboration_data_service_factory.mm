// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_sharing/model/personal_collaboration_data/personal_collaboration_data_service_factory.h"

#import "base/feature_list.h"
#import "base/no_destructor.h"
#import "components/data_sharing/internal/personal_collaboration_data/personal_collaboration_data_service_impl.h"
#import "components/data_sharing/public/features.h"
#import "components/data_sharing/public/personal_collaboration_data/personal_collaboration_data_service.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/sync/base/report_unrecoverable_error.h"
#import "components/sync/model/client_tag_based_data_type_processor.h"
#import "components/sync/model/data_type_store_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/browser_state.h"

namespace data_sharing::personal_collaboration_data {

// static
PersonalCollaborationDataService*
PersonalCollaborationDataServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<PersonalCollaborationDataService>(
          profile, /*create=*/true);
}

// static
PersonalCollaborationDataServiceFactory*
PersonalCollaborationDataServiceFactory::GetInstance() {
  static base::NoDestructor<PersonalCollaborationDataServiceFactory> instance;
  return instance.get();
}

PersonalCollaborationDataServiceFactory::
    PersonalCollaborationDataServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PersonalCollaborationDataService",
                                    ProfileSelection::kNoInstanceInIncognito) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

PersonalCollaborationDataServiceFactory::
    ~PersonalCollaborationDataServiceFactory() = default;

std::unique_ptr<KeyedService>
PersonalCollaborationDataServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  CHECK(!profile->IsOffTheRecord());
  if (!data_sharing::features::IsDataSharingFunctionalityEnabled() ||
      !base::FeatureList::IsEnabled(
          features::kDataSharingAccountDataMigration)) {
    return nullptr;
  }

  version_info::Channel channel = ::GetChannel();
  auto data_type_store_factory =
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();
  return std::make_unique<PersonalCollaborationDataServiceImpl>(
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::SHARED_TAB_GROUP_ACCOUNT_DATA,
          base::BindRepeating(&syncer::ReportUnrecoverableError, channel)),
      data_type_store_factory);
}

}  // namespace data_sharing::personal_collaboration_data
