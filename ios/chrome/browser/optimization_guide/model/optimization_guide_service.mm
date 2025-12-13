// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"

#import "base/apple/bundle_locations.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "base/path_service.h"
#import "base/system/sys_info.h"
#import "base/task/thread_pool.h"
#import "base/time/default_clock.h"
#import "components/component_updater/pref_names.h"
#import "components/optimization_guide/core/delivery/prediction_manager.h"
#import "components/optimization_guide/core/hints/command_line_top_host_provider.h"
#import "components/optimization_guide/core/hints/hints_processing_util.h"
#import "components/optimization_guide/core/hints/optimization_guide_navigation_data.h"
#import "components/optimization_guide/core/hints/optimization_guide_store.h"
#import "components/optimization_guide/core/hints/top_host_provider.h"
#import "components/optimization_guide/core/model_execution/model_execution_features_controller.h"
#import "components/optimization_guide/core/model_execution/model_execution_manager.h"
#import "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#import "components/optimization_guide/core/model_execution/remote_model_executor.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/optimization_guide/core/optimization_guide_logger.h"
#import "components/optimization_guide/core/optimization_guide_util.h"
#import "components/prefs/pref_service.h"
#import "components/services/unzip/in_process_unzipper.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/variations/service/variations_service.h"
#import "components/variations/synthetic_trials.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_service_accessor.h"
#import "ios/chrome/browser/optimization_guide/model/ios_chrome_hints_manager.h"
#import "ios/chrome/browser/optimization_guide/model/ios_model_quality_logs_uploader_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/tab_url_provider_impl.h"
#import "ios/chrome/browser/policy/model/management_service_ios_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

using ModelExecutionError = optimization_guide::
    OptimizationGuideModelExecutionError::ModelExecutionError;

}  // namespace

OptimizationGuideService::OptimizationGuideService(
    leveldb_proto::ProtoDatabaseProvider* proto_db_provider,
    const base::FilePath& profile_path,
    bool off_the_record,
    const std::string& application_locale,
    base::WeakPtr<optimization_guide::OptimizationGuideStore> hint_store,
    PrefService* pref_service,
    BrowserList* browser_list,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : pref_service_(pref_service), off_the_record_(off_the_record) {
  DCHECK(optimization_guide::features::IsOptimizationHintsEnabled());

  // In off the record profile, the stores of normal profile should be
  // passed to the constructor. In normal profile, they will be created.
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
  optimization_guide_logger_ = OptimizationGuideLogger::GetInstance();
  DCHECK(optimization_guide_logger_);
  hints_manager_ = std::make_unique<optimization_guide::IOSChromeHintsManager>(
      off_the_record_, application_locale, pref_service, hint_store,
      top_host_provider_.get(), tab_url_provider_.get(), url_loader_factory,
      identity_manager, optimization_guide_logger_.get());

  if (!off_the_record_) {
    variations::VariationsService* variations_service =
        GetApplicationContext()->GetVariationsService();
    auto dogfood_status =
        variations_service && variations_service->IsLikelyDogfoodClient()
            ? optimization_guide::ModelExecutionFeaturesController::
                  DogfoodStatus::DOGFOOD
            : optimization_guide::ModelExecutionFeaturesController::
                  DogfoodStatus::NON_DOGFOOD;
    model_execution_features_controller_ =
        std::make_unique<optimization_guide::ModelExecutionFeaturesController>(
            pref_service, identity_manager,
            GetApplicationContext()->GetLocalState(),
            policy::ManagementServiceIOSFactory::GetForPlatform(),
            dogfood_status, version_info::IsOfficialBuild());

    if (optimization_guide::features::IsModelQualityLoggingEnabled()) {
      model_quality_logs_uploader_service_ =
          std::make_unique<IOSModelQualityLogsUploaderService>(
              url_loader_factory, GetApplicationContext()->GetLocalState(),
              model_execution_features_controller_->GetWeakPtr());
    }
    model_execution_manager_ =
        std::make_unique<optimization_guide::ModelExecutionManager>(
            url_loader_factory, identity_manager, /*delegate=*/nullptr,
            optimization_guide_logger_.get(),
            model_quality_logs_uploader_service_
                ? model_quality_logs_uploader_service_->GetWeakPtr()
                : nullptr);
  }

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
  if (off_the_record_) {
    return;
  }
  bool optimization_guide_fetching_enabled =
      optimization_guide::IsUserPermittedToFetchFromRemoteOptimizationGuide(
          off_the_record_, pref_service_);
  base::UmaHistogramBoolean("OptimizationGuide.RemoteFetchingEnabled",
                            optimization_guide_fetching_enabled);
  IOSChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      "SyntheticOptimizationGuideRemoteFetching",
      optimization_guide_fetching_enabled ? "Enabled" : "Disabled",
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}

optimization_guide::HintsManager* OptimizationGuideService::GetHintsManager() {
  return hints_manager_.get();
}

optimization_guide::PredictionManager*
OptimizationGuideService::GetPredictionManager() {
  return &GetApplicationContext()
              ->GetOptimizationGuideGlobalState()
              ->prediction_manager();
}

void OptimizationGuideService::AddHintForTesting(
    const GURL& url,
    optimization_guide::proto::OptimizationType optimization_type,
    const std::optional<optimization_guide::OptimizationMetadata>& metadata) {
  hints_manager_->AddHintForTesting(url, optimization_type,  // IN-TEST
                                    metadata);
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

void OptimizationGuideService::CanApplyOptimizationOnDemand(
    const std::vector<GURL>& urls,
    const base::flat_set<optimization_guide::proto::OptimizationType>&
        optimization_types,
    optimization_guide::proto::RequestContext request_context,
    optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback
        callback,
    std::optional<optimization_guide::proto::RequestContextMetadata>
        request_context_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(request_context !=
         optimization_guide::proto::RequestContext::CONTEXT_UNSPECIFIED);

  hints_manager_->CanApplyOptimizationOnDemand(
      urls, optimization_types, request_context, callback, std::nullopt);
}

void OptimizationGuideService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hints_manager_->Shutdown();
}

void OptimizationGuideService::OnBrowsingDataRemoved() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hints_manager_->ClearFetchedHints();
}

std::string OptimizationGuideService::ResponseForErrorCode(int error_code) {
  ModelExecutionError model_execution_error =
      static_cast<ModelExecutionError>(error_code);
  switch (model_execution_error) {
    case ModelExecutionError::kUnknown:
      return "Unknown error (error code 0)";
    case ModelExecutionError::kInvalidRequest:
      return "Invalid request (error code 1)";
    case ModelExecutionError::kRequestThrottled:
      return "Request throttled (error code 2)";
    case ModelExecutionError::kPermissionDenied:
      return "Permission denied (error code 3)";
    case ModelExecutionError::kGenericFailure:
      return "Generic failure (error code 4)";
    case ModelExecutionError::kRetryableError:
      return "Retryable error in server (error code 5)";
    case ModelExecutionError::kNonRetryableError:
      return "Non-retryable error in server (error code 6)";
    case ModelExecutionError::kUnsupportedLanguage:
      return "Unsupported language (error code 7)";
    case ModelExecutionError::kFiltered:
      return "Request was filtered (error code 8)";
    case ModelExecutionError::kDisabled:
      return "Response was disabled (error code 9)";
    case ModelExecutionError::kCancelled:
      return "Response was cancelled (error code 10)";
    case ModelExecutionError::kResponseLowQuality:
      return "Low quality response (error code 11)";
  }
}

#pragma mark - optimization_guide::OptimizationGuideModelProvider implementation

void OptimizationGuideService::AddObserverForOptimizationTargetModel(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const std::optional<optimization_guide::proto::Any>& model_metadata,
    scoped_refptr<base::SequencedTaskRunner> model_task_runner,
    optimization_guide::OptimizationTargetModelObserver* observer) {
  if (optimization_guide::features::IsOptimizationTargetPredictionEnabled()) {
    GetPredictionManager()->AddObserverForOptimizationTargetModel(
        optimization_target, model_metadata, model_task_runner, observer);
  }
}

void OptimizationGuideService::RemoveObserverForOptimizationTargetModel(
    optimization_guide::proto::OptimizationTarget optimization_target,
    optimization_guide::OptimizationTargetModelObserver* observer) {
  if (optimization_guide::features::IsOptimizationTargetPredictionEnabled()) {
    GetPredictionManager()->RemoveObserverForOptimizationTargetModel(
        optimization_target, observer);
  }
}

#pragma mark - optimization_guide::RemoteModelExecutor implementation

void OptimizationGuideService::ExecuteModel(
    optimization_guide::ModelBasedCapabilityKey feature,
    const google::protobuf::MessageLite& request_metadata,
    const optimization_guide::ModelExecutionOptions& options,
    optimization_guide::OptimizationGuideModelExecutionResultCallback
        callback) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (!model_execution_manager_) {
    std::move(callback).Run(
        optimization_guide::OptimizationGuideModelExecutionResult(
            base::unexpected(
                optimization_guide::OptimizationGuideModelExecutionError::
                    FromModelExecutionError(
                        optimization_guide::
                            OptimizationGuideModelExecutionError::
                                ModelExecutionError::kGenericFailure)),
            /*model_execution_info=*/nullptr),
        nullptr);
    return;
  }
  model_execution_manager_->ExecuteModel(
      feature, request_metadata, options.execution_timeout,
      /*log_ai_data_request=*/nullptr, options.service_type,
      std::move(callback));
}
