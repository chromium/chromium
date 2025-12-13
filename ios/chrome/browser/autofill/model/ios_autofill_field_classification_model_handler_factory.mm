// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/ios_autofill_field_classification_model_handler_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "components/autofill/core/browser/ml_model/field_classification_model_handler.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
IOSAutofillFieldClassificationModelHandlerFactory*
IOSAutofillFieldClassificationModelHandlerFactory::GetInstance() {
  static base::NoDestructor<IOSAutofillFieldClassificationModelHandlerFactory>
      instance;
  return instance.get();
}

// static
autofill::FieldClassificationModelHandler*
IOSAutofillFieldClassificationModelHandlerFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<autofill::FieldClassificationModelHandler>(
          profile, /*create=*/true);
}

IOSAutofillFieldClassificationModelHandlerFactory::
    IOSAutofillFieldClassificationModelHandlerFactory()
    : ProfileKeyedServiceFactoryIOS("FieldClassificationModelHandler",
                                    ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

IOSAutofillFieldClassificationModelHandlerFactory::
    ~IOSAutofillFieldClassificationModelHandlerFactory() = default;

std::unique_ptr<KeyedService>
IOSAutofillFieldClassificationModelHandlerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  OptimizationGuideService* optimization_guide =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  if (!optimization_guide) {
    // `FieldClassificationModelHandler` is not supported without an
    // `OptimizationGuideService`.
    return nullptr;
  }
  return std::make_unique<autofill::FieldClassificationModelHandler>(
      optimization_guide,
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION);
}
