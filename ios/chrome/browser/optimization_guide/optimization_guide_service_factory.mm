// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/optimization_guide/optimization_guide_service_factory.h"

#import "base/feature_list.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/optimization_guide/optimization_guide_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
OptimizationGuideService* OptimizationGuideServiceFactory::GetForBrowserState(
    ChromeBrowserState* context) {
  if (!optimization_guide::features::IsOptimizationHintsEnabled())
    return nullptr;
  return static_cast<OptimizationGuideService*>(
      GetInstance()->GetServiceForBrowserState(context, /*create=*/true));
}

// static
OptimizationGuideServiceFactory*
OptimizationGuideServiceFactory::GetInstance() {
  static base::NoDestructor<OptimizationGuideServiceFactory> instance;
  return instance.get();
}

OptimizationGuideServiceFactory::OptimizationGuideServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "OptimizationGuideService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(BrowserListFactory::GetInstance());
}

OptimizationGuideServiceFactory::~OptimizationGuideServiceFactory() = default;

std::unique_ptr<KeyedService>
OptimizationGuideServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<OptimizationGuideService>(context);
}

bool OptimizationGuideServiceFactory::ServiceIsCreatedWithBrowserState() const {
  return optimization_guide::features::IsOptimizationHintsEnabled();
}
