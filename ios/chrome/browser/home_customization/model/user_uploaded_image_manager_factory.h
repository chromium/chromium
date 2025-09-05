// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_USER_UPLOADED_IMAGE_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_USER_UPLOADED_IMAGE_MANAGER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class UserUploadedImageManager;

// Singleton that owns all UserUploadedImageManagers and associates
// them with profiles.
class UserUploadedImageManagerFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static UserUploadedImageManager* GetForProfile(ProfileIOS* profile);
  static UserUploadedImageManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<UserUploadedImageManagerFactory>;

  UserUploadedImageManagerFactory();
  ~UserUploadedImageManagerFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_USER_UPLOADED_IMAGE_MANAGER_FACTORY_H_
