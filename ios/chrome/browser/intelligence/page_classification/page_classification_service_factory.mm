// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_classification/page_classification_service_factory.h"

#import "ios/chrome/browser/intelligence/page_classification/optimization_guide_page_classification_service.h"
#import "ios/chrome/browser/intelligence/page_classification/page_classification_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

std::unique_ptr<KeyedService> BuildPageClassificationService(
    ProfileIOS* profile) {
  CHECK(!profile->IsOffTheRecord());
  return std::make_unique<OptimizationGuidePageClassificationService>(
      OptimizationGuideServiceFactory::GetForProfile(profile));
}

}  // namespace

// static
PageClassificationService* PageClassificationServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<PageClassificationService>(
      profile, /*create=*/true);
}

// static
PageClassificationServiceFactory*
PageClassificationServiceFactory::GetInstance() {
  static base::NoDestructor<PageClassificationServiceFactory> instance;
  return instance.get();
}

// static
ProfileKeyedServiceFactoryIOS::TestingFactory
PageClassificationServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildPageClassificationService);
}

PageClassificationServiceFactory::PageClassificationServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PageClassificationService") {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
}

PageClassificationServiceFactory::~PageClassificationServiceFactory() = default;

std::unique_ptr<KeyedService>
PageClassificationServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildPageClassificationService(profile);
}
