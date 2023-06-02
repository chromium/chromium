// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class OptimizationGuideService;

// Singleton that owns all OptimizationGuideService(s) and associates them
// with ChromeBrowserState.
class OptimizationGuideServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static OptimizationGuideService* GetForBrowserState(
      ChromeBrowserState* context);
  static OptimizationGuideServiceFactory* GetInstance();

  // Initializes the prediction model store.
  static void InitializePredictionModelStore();

  // Returns the default factory used to build OptimizationGuideService. Can be
  // registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<OptimizationGuideServiceFactory>;

  OptimizationGuideServiceFactory();
  ~OptimizationGuideServiceFactory() override;
  OptimizationGuideServiceFactory(const OptimizationGuideServiceFactory&) =
      delete;
  OptimizationGuideServiceFactory& operator=(
      const OptimizationGuideServiceFactory&) = delete;

  // TODO(crbug.com/1232860): Add handling for Incognito browsers. For now, the
  // default behavior of a null service for incognito browsers is ok.
  //
  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsCreatedWithBrowserState() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_FACTORY_H_
