// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service_factory.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/intelligence/on_device_category_classifier/in_process_category_classification_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

std::unique_ptr<KeyedService> BuildInProcessCategoryClassificationService(
    ProfileIOS* profile) {
  if (profile->IsOffTheRecord()) {
    return nullptr;
  }
  OptimizationGuideService* opt_guide =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  if (!opt_guide) {
    return nullptr;
  }

  return std::make_unique<InProcessCategoryClassificationService>(opt_guide);
}

}  // namespace

// static
InProcessCategoryClassificationService*
InProcessCategoryClassificationServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<InProcessCategoryClassificationService>(
          profile, /*create=*/true);
}

// static
InProcessCategoryClassificationServiceFactory*
InProcessCategoryClassificationServiceFactory::GetInstance() {
  static base::NoDestructor<InProcessCategoryClassificationServiceFactory>
      instance;
  return instance.get();
}

// static
ProfileKeyedServiceFactoryIOS::TestingFactory
InProcessCategoryClassificationServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildInProcessCategoryClassificationService);
}

InProcessCategoryClassificationServiceFactory::
    InProcessCategoryClassificationServiceFactory()
    : ProfileKeyedServiceFactoryIOS("InProcessCategoryClassificationService",
                                    ProfileSelection::kOwnInstanceInIncognito,
                                    ServiceCreation::kCreateLazily,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

InProcessCategoryClassificationServiceFactory::
    ~InProcessCategoryClassificationServiceFactory() = default;

std::unique_ptr<KeyedService>
InProcessCategoryClassificationServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildInProcessCategoryClassificationService(profile);
}
