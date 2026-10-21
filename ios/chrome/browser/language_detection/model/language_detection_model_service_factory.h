// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LANGUAGE_DETECTION_MODEL_LANGUAGE_DETECTION_MODEL_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_LANGUAGE_DETECTION_MODEL_LANGUAGE_DETECTION_MODEL_SERVICE_FACTORY_H_

#import <memory>

#import "components/language_detection/core/browser/language_detection_model_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Owns all LanguageDetectionModelService instances and associte them to
// profiles.
class LanguageDetectionModelServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          LanguageDetectionModelServiceFactory,
          language_detection::LanguageDetectionModelService> {
 public:
  explicit LanguageDetectionModelServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_LANGUAGE_DETECTION_MODEL_LANGUAGE_DETECTION_MODEL_SERVICE_FACTORY_H_
