// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_classification/page_classification_service_factory.h"

#import <utility>

#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
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
      OptimizationGuideServiceFactory::GetForProfile(profile),
      commerce::ShoppingServiceFactory::GetForProfile(profile));
}

}  // namespace

// static
PageClassificationServiceFactory::TestingFactory
PageClassificationServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildPageClassificationService);
}

PageClassificationServiceFactory::PageClassificationServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "PageClassificationService") {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
  DependsOn(commerce::ShoppingServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
PageClassificationServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildPageClassificationService(profile);
}
