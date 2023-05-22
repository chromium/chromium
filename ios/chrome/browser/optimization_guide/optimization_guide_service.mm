// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/optimization_guide_service.h"

#import "base/files/file_util.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_functions.h"
#import "base/path_service.h"
#import "base/task/thread_pool.h"
#import "base/time/default_clock.h"
#import "components/component_updater/pref_names.h"
#import "components/optimization_guide/core/command_line_top_host_provider.h"
#import "components/optimization_guide/core/hints_processing_util.h"
#import "components/optimization_guide/core/optimization_guide_constants.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/optimization_guide/core/optimization_guide_logger.h"
#import "components/optimization_guide/core/optimization_guide_navigation_data.h"
#import "components/optimization_guide/core/optimization_guide_store.h"
#import "components/optimization_guide/core/optimization_guide_util.h"
#import "components/optimization_guide/core/prediction_manager.h"
#import "components/optimization_guide/core/top_host_provider.h"
#import "components/prefs/pref_service.h"
#import "components/variations/synthetic_trials.h"
#import "ios/chrome/browser/metrics/ios_chrome_metrics_service_accessor.h"
#import "ios/chrome/browser/optimization_guide/ios_chrome_hints_manager.h"
#import "ios/chrome/browser/optimization_guide/optimization_guide_service_factory.h"
#import "ios/chrome/browser/optimization_guide/tab_url_provider_impl.h"
#import "ios/chrome/browser/paths/paths.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kOldOptimizationGuidePredictionModelAndFeaturesStore[] =
    "optimization_guide_model_and_features_store";

// Deletes old store paths that were written in incorrect locations.
void DeleteOldStorePaths(const base::FilePath& profile_path) {
  // Added 05/2022.

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::GetDeletePathRecursivelyCallback(profile_path.Append(
          kOldOptimizationGuidePredictionModelAndFeaturesStore)));

  base::FilePath models_dir;
  base::PathService::Get(ios::DIR_OPTIMIZATION_GUIDE_PREDICTION_MODELS,
                         &models_dir);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::GetDeletePathRecursivelyCallback(models_dir));
}

}  // namespace

OptimizationGuideService::OptimizationGuideService(
    leveldb_proto::ProtoDatabaseProvider* proto_db_provider,
    const base::FilePath& profile_path,
    bool off_the_record,
    const std::string& application_locale,
    base::WeakPtr<optimization_guide::OptimizationGuideStore> hint_store,
    base::WeakPtr<optimization_guide::OptimizationGuideStore>
        prediction_model_and_features_store,
    PrefService* pref_service,
    BrowserList* browser_list,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    BackgroundDownloadServiceProvider background_download_service_provider)
    : pref_service_(pref_service), off_the_record_(off_the_record) {
  DCHECK(optimization_guide::features::IsOptimizationHintsEnabled());

  // In off the record profile, the stores of normal profile should be
  // passed to the constructor. In normal profile, they will be created.
  DCHECK(!off_the_record_ || hint_store);
  if (off_the_record_ &&
      optimization_guide::features::IsOptimizationTargetPredictionEnabled()) {
    DCHECK(prediction_model_and_features_store ||
           optimization_guide::features::IsInstallWideModelStoreEnabled());
  }
  base::FilePath models_dir;
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
                      {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
                  pref_service)
            : nullptr;
    hint_store = hint_store_ ? hint_store_->AsWeakPtr() : nullptr;
    if (optimization_guide::features::IsOptimizationTargetPredictionEnabled() &&
        !optimization_guide::features::IsInstallWideModelStoreEnabled()) {
      // Do not explicitly hand off the model downloads directory to
      // off-the-record profiles. Underneath the hood, this variable is only
      // used in non off-the-record profiles to know where to download the model
      // files to. Off-the-record profiles read the model locations from the
      // original profiles they are associated with.
      models_dir = profile_path.Append(
          optimization_guide::kOptimizationGuidePredictionModelDownloads);
      prediction_model_and_features_store_ =
          std::make_unique<optimization_guide::OptimizationGuideStore>(
              proto_db_provider,
              profile_path.Append(
                  optimization_guide::
                      kOptimizationGuidePredictionModelMetadataStore),
              models_dir,
              base::ThreadPool::CreateSequencedTaskRunner(
                  {base::MayBlock(), base::TaskPriority::BEST_EFFORT}),
              pref_service);
      prediction_model_and_features_store =
          prediction_model_and_features_store_->AsWeakPtr();
    }
  }
  optimization_guide_logger_ = std::make_unique<OptimizationGuideLogger>();
  hints_manager_ = std::make_unique<optimization_guide::IOSChromeHintsManager>(
      off_the_record_, application_locale, pref_service, hint_store,
      top_host_provider_.get(), tab_url_provider_.get(), url_loader_factory,
      optimization_guide_logger_.get());

  if (optimization_guide::features::IsOptimizationTargetPredictionEnabled()) {
    prediction_manager_ =
        std::make_unique<optimization_guide::PredictionManager>(
            optimization_guide::features::IsInstallWideModelStoreEnabled()
                ? nullptr
                : prediction_model_and_features_store,
            optimization_guide::features::IsInstallWideModelStoreEnabled()
                ? optimization_guide::PredictionModelStore::GetInstance()
                : nullptr,
            url_loader_factory, pref_service, off_the_record_,
            application_locale, models_dir, optimization_guide_logger_.get(),
            std::move(background_download_service_provider),
            base::BindRepeating([]() {
              return GetApplicationContext()->GetLocalState()->GetBoolean(
                  ::prefs::kComponentUpdatesEnabled);
            }));
  }

  // Some previous paths were written in incorrect locations. Delete the
  // old paths.
  //
  // TODO(crbug.com/1328981): Remove this code in 05/2023 since it should be
  // assumed that all clients that had the previous path have had their previous
  // stores deleted.
  DeleteOldStorePaths(profile_path);

  OPTIMIZATION_GUIDE_LOG(
      optimization_guide_common::mojom::LogSource::SERVICE_AND_SETTINGS,
      optimization_guide_logger_,
      "OptimizationGuide: KeyedService is initalized");

  optimization_guide::LogFeatureFlagsInfo(optimization_guide_logger_.get(),
                                          off_the_record_, pref_service);
}

OptimizationGuideService::~OptimizationGuideService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OptimizationGuideService::DoFinalInit(
    download::BackgroundDownloadService* background_download_service) {
  if (!off_the_record_) {
    bool optimization_guide_fetching_enabled =
        optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
            off_the_record_, pref_service_);
    base::UmaHistogramBoolean("OptimizationGuide.RemoteFetchingEnabled",
                              optimization_guide_fetching_enabled);
    IOSChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        "SyntheticOptimizationGuideRemoteFetching",
        optimization_guide_fetching_enabled ? "Enabled" : "Disabled",
        variations::SyntheticTrialAnnotationMode::kCurrentLog);
    if (optimization_guide::features::IsInstallWideModelStoreEnabled() &&
        background_download_service) {
      prediction_manager_->MaybeInitializeModelDownloads(
          background_download_service);
    }
  }
}

optimization_guide::HintsManager* OptimizationGuideService::GetHintsManager() {
  return hints_manager_.get();
}

optimization_guide::PredictionManager*
OptimizationGuideService::GetPredictionManager() {
  return prediction_manager_.get();
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

// WARNING: This API is not quite ready for general use. Use
// CanApplyOptimizationAsync or CanApplyOptimization using NavigationHandle
// instead.
void OptimizationGuideService::CanApplyOptimization(
    const GURL& url,
    optimization_guide::proto::OptimizationType optimization_type,
    optimization_guide::OptimizationGuideDecisionCallback callback) {
  hints_manager_->CanApplyOptimization(url, optimization_type,
                                       std::move(callback));
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

void OptimizationGuideService::CanApplyOptimizationOnDemand(
    const std::vector<GURL>& urls,
    const base::flat_set<optimization_guide::proto::OptimizationType>&
        optimization_types,
    optimization_guide::proto::RequestContext request_context,
    optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(request_context !=
         optimization_guide::proto::RequestContext::CONTEXT_UNSPECIFIED);

  hints_manager_->CanApplyOptimizationOnDemand(urls, optimization_types,
                                               request_context, callback);
}

void OptimizationGuideService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hints_manager_->Shutdown();
}

void OptimizationGuideService::OnBrowsingDataRemoved() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hints_manager_->ClearFetchedHints();
}

#pragma mark - optimization_guide::OptimizationGuideModelProvider implementation
void OptimizationGuideService::AddObserverForOptimizationTargetModel(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const absl::optional<optimization_guide::proto::Any>& model_metadata,
    optimization_guide::OptimizationTargetModelObserver* observer) {
  if (optimization_guide::features::IsOptimizationTargetPredictionEnabled()) {
    prediction_manager_->AddObserverForOptimizationTargetModel(
        optimization_target, model_metadata, observer);
  }
}

void OptimizationGuideService::RemoveObserverForOptimizationTargetModel(
    optimization_guide::proto::OptimizationTarget optimization_target,
    optimization_guide::OptimizationTargetModelObserver* observer) {
  if (optimization_guide::features::IsOptimizationTargetPredictionEnabled()) {
    prediction_manager_->RemoveObserverForOptimizationTargetModel(
        optimization_target, observer);
  }
}
