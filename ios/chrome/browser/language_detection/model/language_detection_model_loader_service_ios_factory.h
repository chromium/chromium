// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LANGUAGE_DETECTION_MODEL_LANGUAGE_DETECTION_MODEL_LOADER_SERVICE_IOS_FACTORY_H_
#define IOS_CHROME_BROWSER_LANGUAGE_DETECTION_MODEL_LANGUAGE_DETECTION_MODEL_LOADER_SERVICE_IOS_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace language_detection {
class LanguageDetectionModelLoaderServiceIOS;
}  // namespace language_detection

// This is a workaround for crbug/1324530 on iOS where it is mandatory to have
// LanguageDetectionModel scoped by Profile.
// TODO(crbug.com/40225076): remove this class once
// LanguageDetectionModelService does this.
class LanguageDetectionModelLoaderServiceIOSFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static language_detection::LanguageDetectionModelLoaderServiceIOS*
  GetForProfile(ProfileIOS* profile);

  static LanguageDetectionModelLoaderServiceIOSFactory* GetInstance();

 private:
  friend class base::NoDestructor<
      LanguageDetectionModelLoaderServiceIOSFactory>;

  LanguageDetectionModelLoaderServiceIOSFactory();
  ~LanguageDetectionModelLoaderServiceIOSFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_LANGUAGE_DETECTION_MODEL_LANGUAGE_DETECTION_MODEL_LOADER_SERVICE_IOS_FACTORY_H_
