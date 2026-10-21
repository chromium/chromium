// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_SHARING_MODEL_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DATA_SHARING_MODEL_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_FACTORY_H_

#import "components/data_sharing/public/personal_collaboration_data/personal_collaboration_data_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace data_sharing::personal_collaboration_data {

// Factory for PersonalCollaborationDataService.
class PersonalCollaborationDataServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          PersonalCollaborationDataServiceFactory,
          PersonalCollaborationDataService> {
 public:
  explicit PersonalCollaborationDataServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace data_sharing::personal_collaboration_data

#endif  // IOS_CHROME_BROWSER_DATA_SHARING_MODEL_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_FACTORY_H_
