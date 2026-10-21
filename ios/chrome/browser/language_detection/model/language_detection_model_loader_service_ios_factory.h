// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LANGUAGE_DETECTION_MODEL_LANGUAGE_DETECTION_MODEL_LOADER_SERVICE_IOS_FACTORY_H_
#define IOS_CHROME_BROWSER_LANGUAGE_DETECTION_MODEL_LANGUAGE_DETECTION_MODEL_LOADER_SERVICE_IOS_FACTORY_H_

#import <memory>

#import "components/language_detection/ios/browser/language_detection_model_loader_service_ios.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// This is a workaround for crbug/1324530 on iOS where it is mandatory to have
// LanguageDetectionModel scoped by Profile.
// TODO(crbug.com/40225076): remove this class once
// LanguageDetectionModelService does this.
class LanguageDetectionModelLoaderServiceIOSFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          LanguageDetectionModelLoaderServiceIOSFactory,
          language_detection::LanguageDetectionModelLoaderServiceIOS> {
 public:
  explicit LanguageDetectionModelLoaderServiceIOSFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_LANGUAGE_DETECTION_MODEL_LANGUAGE_DETECTION_MODEL_LOADER_SERVICE_IOS_FACTORY_H_
