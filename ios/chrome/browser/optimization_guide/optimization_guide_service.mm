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
#import "components/optimization_guide/core/optimization_guide_logger.h"
#import "components/optimization_guide/core/optimization_guide_navigation_data.h"
#import "components/optimization_guide/core/optimization_guide_permissions_util.h"
#import "components/optimization_guide/core/optimization_guide_store.h"
#import "components/optimization_guide/core/optimization_guide_util.h"
#import "components/optimization_guide/core/top_host_provider.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
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
    leveldb_proto::ProtoDatabaseProvider* proto_db_provider,
    const base::FilePath& profile_path,
    bool off_the_record,
    const std::string& application_locale,
    base::WeakPtr<optimization_guide::OptimizationGuideStore> hint_store,
    PrefService* pref_service,
    BrowserList* browser_list,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : pref_service_(pref_service), off_the_record_(off_the_record) {
  DCHECK(optimization_guide::features::IsOptimizationHintsEnabled());

  base::WeakPtr<optimization_guide::OptimizationGuideStore>
      prediction_model_and_features_store;
  DCHECK(!off_the_record_ || hint_store);
  if (!off_the_record_) {
    // Only create a top host provider from the command line if provided.
    top_host_provider_ =
        optimization_guide::CommandLineTopHostProvider::CreateIfEnabled();
    tab_url_provider_ = std::make_unique<TabUrlProviderImpl>(
        browser_list, base::DefaultClock::GetInstance());
    hint_store_ =
        optimization_guide::features::ShouldPersistHintsToDisk()
            ? std::make_unique<optimization_guide::OptimizationGuideStore>(
                  proto_db_provider,
                  profile_path.Append(
                      optimization_guide::kOptimizationGuideHintStore),
                  base::ThreadPool::CreateSequencedTaskRunner(
                      {base::MayBlock(), base::TaskPriority::BEST_EFFORT}))
            : nullptr;
    hint_store = hint_store_ ? hint_store_->AsWeakPtr() : nullptr;
  }
  optimization_guide_logger_ = std::make_unique<OptimizationGuideLogger>();
  hints_manager_ = std::make_unique<optimization_guide::IOSChromeHintsManager>(
      off_the_record_, application_locale, pref_service, hint_store,
      top_host_provider_.get(), tab_url_provider_.get(), url_loader_factory,
      optimization_guide_logger_.get());
}

OptimizationGuideService::~OptimizationGuideService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OptimizationGuideService::DoFinalInit() {
  if (!off_the_record_) {
    bool optimization_guide_fetching_enabled =
        optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
            off_the_record_, pref_service_);
    base::UmaHistogramBoolean("OptimizationGuide.RemoteFetchingEnabled",
                              optimization_guide_fetching_enabled);
    IOSChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        "SyntheticOptimizationGuideRemoteFetching",
        optimization_guide_fetching_enabled ? "Enabled" : "Disabled");
  }
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
      hints_manager_->CanApplyOptimization(url, optimization_type,
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
      navigation_context->GetUrl(), optimization_type, std::move(callback));
}

void OptimizationGuideService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hints_manager_->Shutdown();
}

void OptimizationGuideService::OnBrowsingDataRemoved() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hints_manager_->ClearFetchedHints();
}
