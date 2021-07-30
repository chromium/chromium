// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/optimization_guide/optimization_guide_service_factory.h"

#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/optimization_guide/optimization_guide_service.h"

// static
OptimizationGuideService* OptimizationGuideServiceFactory::GetForBrowserState(
    ChromeBrowserState* context) {
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
          BrowserStateDependencyManager::GetInstance()) {}

OptimizationGuideServiceFactory::~OptimizationGuideServiceFactory() = default;

std::unique_ptr<KeyedService>
OptimizationGuideServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<OptimizationGuideService>();
}
