// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_SHARING_MODEL_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DATA_SHARING_MODEL_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace data_sharing::personal_collaboration_data {

class PersonalCollaborationDataService;

// Factory for PersonalCollaborationDataService.
class PersonalCollaborationDataServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static PersonalCollaborationDataService* GetForProfile(ProfileIOS* profile);

  static PersonalCollaborationDataServiceFactory* GetInstance();

  PersonalCollaborationDataServiceFactory(
      const PersonalCollaborationDataServiceFactory&) = delete;
  PersonalCollaborationDataServiceFactory& operator=(
      const PersonalCollaborationDataServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<PersonalCollaborationDataServiceFactory>;

  PersonalCollaborationDataServiceFactory();
  ~PersonalCollaborationDataServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace data_sharing::personal_collaboration_data

#endif  // IOS_CHROME_BROWSER_DATA_SHARING_MODEL_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_FACTORY_H_
