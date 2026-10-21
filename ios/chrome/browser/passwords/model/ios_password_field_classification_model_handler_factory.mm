// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_password_field_classification_model_handler_factory.h"

#import <memory>
#import <utility>

#import "components/autofill/core/browser/ml_model/field_classification_model_handler.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

IOSPasswordFieldClassificationModelHandlerFactory::
    IOSPasswordFieldClassificationModelHandlerFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "FieldClassificationModelHandler",
          ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSPasswordFieldClassificationModelHandlerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  OptimizationGuideService* optimization_guide =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  if (!optimization_guide) {
    // `FieldClassificationModelHandler` is not supported without an
    // `OptimizationGuideService`.
    return nullptr;
  }

  if (!base::FeatureList::IsEnabled(
          password_manager::features::kPasswordFormClientsideClassifier)) {
    return nullptr;
  }

  return std::make_unique<autofill::FieldClassificationModelHandler>(
      optimization_guide,
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_PASSWORD_MANAGER_FORM_CLASSIFICATION);
}
