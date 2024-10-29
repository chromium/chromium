// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_password_field_classification_model_handler_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/browser_state.h"

// static
IOSPasswordFieldClassificationModelHandlerFactory*
IOSPasswordFieldClassificationModelHandlerFactory::GetInstance() {
  static base::NoDestructor<IOSPasswordFieldClassificationModelHandlerFactory>
      instance;
  return instance.get();
}

// static
autofill::FieldClassificationModelHandler*
IOSPasswordFieldClassificationModelHandlerFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<autofill::FieldClassificationModelHandler*>(
      GetInstance()->GetServiceForBrowserState(profile, /*create=*/true));
}

IOSPasswordFieldClassificationModelHandlerFactory::
    IOSPasswordFieldClassificationModelHandlerFactory()
    : BrowserStateKeyedServiceFactory(
          "FieldClassificationModelHandler",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

IOSPasswordFieldClassificationModelHandlerFactory::
    ~IOSPasswordFieldClassificationModelHandlerFactory() = default;

web::BrowserState*
IOSPasswordFieldClassificationModelHandlerFactory::GetBrowserStateToUse(
    web::BrowserState* state) const {
  // `FieldClassificationModelHandler` is not supported without an
  // `OptimizationGuideService`.
  return OptimizationGuideServiceFactory::GetForProfile(
             ProfileIOS::FromBrowserState(state))
             ? state
             : nullptr;
}

std::unique_ptr<KeyedService>
IOSPasswordFieldClassificationModelHandlerFactory::BuildServiceInstanceFor(
    web::BrowserState* state) const {
  OptimizationGuideService* optimization_guide =
      OptimizationGuideServiceFactory::GetForProfile(
          ProfileIOS::FromBrowserState(state));
  return std::make_unique<autofill::FieldClassificationModelHandler>(
      optimization_guide,
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_PASSWORD_MANAGER_FORM_CLASSIFICATION);
}
