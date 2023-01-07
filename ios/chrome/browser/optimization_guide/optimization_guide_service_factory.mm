// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/optimization_guide_service_factory.h"

#import "base/feature_list.h"
#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/optimization_guide/core/prediction_manager.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/optimization_guide/ios_chrome_hints_manager.h"
#import "ios/chrome/browser/optimization_guide/optimization_guide_service.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

std::unique_ptr<KeyedService> BuildOptimizationGuideService(
    web::BrowserState* context) {
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(context);
  ChromeBrowserState* original_browser_state =
      chrome_browser_state->GetOriginalChromeBrowserState();
  DCHECK(chrome_browser_state);
  // Regardless of whether the profile is off the record or not, initialize the
  // Optimization Guide with the database associated with the original profile.
  auto* proto_db_provider = original_browser_state->GetProtoDatabaseProvider();
  base::FilePath profile_path = original_browser_state->GetStatePath();

  base::WeakPtr<optimization_guide::OptimizationGuideStore> hint_store;
  base::WeakPtr<optimization_guide::OptimizationGuideStore>
      prediction_model_and_features_store;
  if (chrome_browser_state->IsOffTheRecord()) {
    OptimizationGuideService* original_ogs =
        OptimizationGuideServiceFactory::GetForBrowserState(
            original_browser_state);
    DCHECK(original_ogs);
    hint_store = original_ogs->GetHintsManager()->hint_store();
    if (optimization_guide::features::IsOptimizationTargetPredictionEnabled()) {
      prediction_model_and_features_store =
          original_ogs->GetPredictionManager()->model_and_features_store();
    }
  }

  return std::make_unique<OptimizationGuideService>(
      proto_db_provider, profile_path, chrome_browser_state->IsOffTheRecord(),
      GetApplicationContext()->GetApplicationLocale(), hint_store,
      prediction_model_and_features_store, chrome_browser_state->GetPrefs(),
      BrowserListFactory::GetForBrowserState(chrome_browser_state),
      chrome_browser_state->GetSharedURLLoaderFactory(),
      base::BindOnce(
          [](ChromeBrowserState* browser_state) {
            return BackgroundDownloadServiceFactory::GetForBrowserState(
                browser_state);
          },
          // base::Unretained is safe here because the callback is owned
          // by PredictionManager which is a transitively owned by
          // OptimizationGuideService (a keyed service that is
          // killed before ChromeBrowserState is deallocated).
          base::Unretained(chrome_browser_state)));
}
}

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
  DependsOn(BackgroundDownloadServiceFactory::GetInstance());
  DependsOn(BrowserListFactory::GetInstance());
}

OptimizationGuideServiceFactory::~OptimizationGuideServiceFactory() = default;

// static
BrowserStateKeyedServiceFactory::TestingFactory
OptimizationGuideServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildOptimizationGuideService);
}

std::unique_ptr<KeyedService>
OptimizationGuideServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildOptimizationGuideService(context);
}

bool OptimizationGuideServiceFactory::ServiceIsCreatedWithBrowserState() const {
  return optimization_guide::features::IsOptimizationHintsEnabled();
}

web::BrowserState* OptimizationGuideServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}

bool OptimizationGuideServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
