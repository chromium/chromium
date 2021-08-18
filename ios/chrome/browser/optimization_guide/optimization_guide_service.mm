// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/optimization_guide/optimization_guide_service.h"

#import "base/callback.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/thread_pool.h"
#import "base/time/default_clock.h"
#import "components/optimization_guide/core/command_line_top_host_provider.h"
#import "components/optimization_guide/core/hints_processing_util.h"
#import "components/optimization_guide/core/optimization_guide_constants.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/optimization_guide/core/optimization_guide_navigation_data.h"
#import "components/optimization_guide/core/optimization_guide_permissions_util.h"
#import "components/optimization_guide/core/optimization_guide_store.h"
#import "components/optimization_guide/core/optimization_guide_util.h"
#import "components/optimization_guide/core/top_host_provider.h"
#import "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/metrics/ios_chrome_metrics_service_accessor.h"
#import "ios/chrome/browser/optimization_guide/ios_chrome_hints_manager.h"
#import "ios/chrome/browser/optimization_guide/optimization_guide_service_factory.h"
#import "ios/chrome/browser/optimization_guide/tab_url_provider_impl.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OptimizationGuideService::OptimizationGuideService(
    web::BrowserState* browser_state) {
  DCHECK(optimization_guide::features::IsOptimizationHintsEnabled());

  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(browser_state);
  DCHECK(chrome_browser_state);

  // TODO(crbug.com/1239388): Handle incognito profile in IOS, the same way its
  // handled in other platforms.
  DCHECK(!browser_state->IsOffTheRecord());

  // Regardless of whether the profile is off the record or not, initialize the
  // Optimization Guide with the database associated with the original profile.
  auto* proto_db_provider =
      chrome_browser_state->GetOriginalChromeBrowserState()
          ->GetProtoDatabaseProvider();
  base::FilePath profile_path =
      chrome_browser_state->GetOriginalChromeBrowserState()->GetStatePath();

  // Only create a top host provider from the command line if provided.
  top_host_provider_ =
      optimization_guide::CommandLineTopHostProvider::CreateIfEnabled();
  tab_url_provider_ = std::make_unique<TabUrlProviderImpl>(
      chrome_browser_state, base::DefaultClock::GetInstance());

  hint_store_ =
      optimization_guide::features::ShouldPersistHintsToDisk()
          ? std::make_unique<optimization_guide::OptimizationGuideStore>(
                proto_db_provider,
                profile_path.Append(
                    optimization_guide::kOptimizationGuideHintStore),
                base::ThreadPool::CreateSequencedTaskRunner(
                    {base::MayBlock(), base::TaskPriority::BEST_EFFORT}))
          : nullptr;

  hints_manager_ = std::make_unique<optimization_guide::IOSChromeHintsManager>(
      browser_state, chrome_browser_state->GetPrefs(), hint_store_.get(),
      top_host_provider_.get(), tab_url_provider_.get(),
      browser_state->GetSharedURLLoaderFactory(),
      GetApplicationContext()->GetNetworkConnectionTracker());

  bool optimization_guide_fetching_enabled =
      optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
          browser_state->IsOffTheRecord(), chrome_browser_state->GetPrefs());
  base::UmaHistogramBoolean("OptimizationGuide.RemoteFetchingEnabled",
                            optimization_guide_fetching_enabled);
  IOSChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      "SyntheticOptimizationGuideRemoteFetching",
      optimization_guide_fetching_enabled ? "Enabled" : "Disabled");
}

OptimizationGuideService::~OptimizationGuideService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

optimization_guide::HintsManager* OptimizationGuideService::GetHintsManager() {
  return hints_manager_.get();
}

void OptimizationGuideService::OnNavigationStartOrRedirect(
    OptimizationGuideNavigationData* navigation_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(navigation_data);

  base::flat_set<optimization_guide::proto::OptimizationType>
      registered_optimization_types =
          hints_manager_->registered_optimization_types();
  if (!registered_optimization_types.empty()) {
    hints_manager_->OnNavigationStartOrRedirect(navigation_data,
                                                base::DoNothing());
  }

  navigation_data->set_registered_optimization_types(
      hints_manager_->registered_optimization_types());
}

void OptimizationGuideService::OnNavigationFinish(
    const std::vector<GURL>& navigation_redirect_chain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  hints_manager_->OnNavigationFinish(navigation_redirect_chain);
}

void OptimizationGuideService::RegisterOptimizationTypes(
    const std::vector<optimization_guide::proto::OptimizationType>&
        optimization_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hints_manager_->RegisterOptimizationTypes(optimization_types);
}

optimization_guide::OptimizationGuideDecision
OptimizationGuideService::CanApplyOptimization(
    const GURL& url,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationMetadata* optimization_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  optimization_guide::OptimizationTypeDecision optimization_type_decision =
      hints_manager_->CanApplyOptimization(url, /*navigation_id=*/absl::nullopt,
                                           optimization_type,
                                           optimization_metadata);
  base::UmaHistogramEnumeration(
      "OptimizationGuide.ApplyDecision." +
          optimization_guide::GetStringNameForOptimizationType(
              optimization_type),
      optimization_type_decision);
  return optimization_guide::HintsManager::
      GetOptimizationGuideDecisionFromOptimizationTypeDecision(
          optimization_type_decision);
}

void OptimizationGuideService::CanApplyOptimizationAsync(
    web::NavigationContext* navigation_context,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationGuideDecisionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  hints_manager_->CanApplyOptimizationAsync(
      navigation_context->GetUrl(), navigation_context->GetNavigationId(),
      optimization_type, std::move(callback));
}

void OptimizationGuideService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hints_manager_->Shutdown();
}
