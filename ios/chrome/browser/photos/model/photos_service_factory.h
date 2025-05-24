// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class PhotosService;

// Singleton that owns all PhotosService-s and associates them with
// Profile.
class PhotosServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static PhotosService* GetForProfile(ProfileIOS* profile);
  static PhotosServiceFactory* GetInstance();

  // Returns the default factory.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<PhotosServiceFactory>;

  PhotosServiceFactory();
  ~PhotosServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PHOTOS_MODEL_PHOTOS_SERVICE_FACTORY_H_
