// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_SERVICE_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_SERVICE_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#import "components/optimization_guide/optimization_guide_buildflags.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "ios/chrome/browser/download/model/background_service/background_download_service_factory.h"
#include "url/gurl.h"

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
#include "components/optimization_guide/core/model_execution/model_execution_features_controller.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
namespace optimization_guide {
class ModelExecutionManager;
class OnDeviceModelAvailabilityObserver;
class OnDeviceModelComponentStateManager;
}  // namespace optimization_guide
#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

namespace optimization_guide {
class HintsManager;
class OptimizationGuideStore;
class OptimizationTargetModelObserver;
class PredictionManager;
class TabUrlProvider;
class TopHostProvider;
}  // namespace optimization_guide

namespace signin {
class IdentityManager;
}  // namespace signin

class BrowserList;
class OptimizationGuideLogger;
class OptimizationGuideNavigationData;
class PrefService;

// A BrowserState keyed service that is used to own the underlying Optimization
// Guide components. This is a rough copy of the OptimizationGuideKeyedService
// in //chrome/browser that is used for non-iOS. It cannot be directly used due
// to the platform differences of the common data structures -
// NavigationContext vs NavigationHandle, BrowserState vs Profile, etc.
// TODO(crbug.com/40785700): Add support for clearing the hints when browsing
// data is cleared.
class OptimizationGuideService
    : public KeyedService,
      public optimization_guide::OptimizationGuideDecider,
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
      public optimization_guide::OptimizationGuideModelExecutor,
#endif
      public optimization_guide::OptimizationGuideModelProvider {
 public:
  // BackgroundDownloadService is only available once the profile is fully
  // initialized and that cannot be done as part of `Initialize`. Get a provider
  // to retrieve the service when it is needed.
  using BackgroundDownloadServiceProvider =
      base::OnceCallback<download::BackgroundDownloadService*(void)>;
  OptimizationGuideService(
      leveldb_proto::ProtoDatabaseProvider* proto_db_provider,
      const base::FilePath& profile_path,
      bool off_the_record,
      const std::string& application_locale,
      base::WeakPtr<optimization_guide::OptimizationGuideStore> hint_store,
      PrefService* pref_service,
      BrowserList* browser_list,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      BackgroundDownloadServiceProvider background_download_service_provider,
      signin::IdentityManager* identity_manager);
  ~OptimizationGuideService() override;

  OptimizationGuideService(const OptimizationGuideService&) = delete;
  OptimizationGuideService& operator=(const OptimizationGuideService&) = delete;

  // Some initialization parts must be done once the profile is fully
  // initialized.
  void DoFinalInit(download::BackgroundDownloadService*
                       background_download_service = nullptr);

  // optimization_guide::OptimizationGuideDecider implementation:
  void RegisterOptimizationTypes(
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types) override;
  void CanApplyOptimization(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationGuideDecisionCallback callback) override;
  optimization_guide::OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationMetadata* optimization_metadata) override;

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  // optimization_guide::OptimizationGuideModelExecutor implementation:
  bool CanCreateOnDeviceSession(
      optimization_guide::ModelBasedCapabilityKey feature,
      optimization_guide::OnDeviceModelEligibilityReason*
          on_device_model_eligibility_reason) override;
  std::unique_ptr<Session> StartSession(
      optimization_guide::ModelBasedCapabilityKey feature,
      const std::optional<optimization_guide::SessionConfigParams>&
          config_params) override;
  void ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey feature,
      const google::protobuf::MessageLite& request_metadata,
      optimization_guide::OptimizationGuideModelExecutionResultCallback
          callback) override;
  void AddOnDeviceModelAvailabilityChangeObserver(
      optimization_guide::ModelBasedCapabilityKey feature,
      optimization_guide::OnDeviceModelAvailabilityObserver* observer) override;
  void RemoveOnDeviceModelAvailabilityChangeObserver(
      optimization_guide::ModelBasedCapabilityKey feature,
      optimization_guide::OnDeviceModelAvailabilityObserver* observer) override;
#endif

  // optimization_guide::OptimizationGuideModelProvider implementation:
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const std::optional<optimization_guide::proto::Any>& model_metadata,
      optimization_guide::OptimizationTargetModelObserver* observer) override;
  void RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      optimization_guide::OptimizationTargetModelObserver* observer) override;

  // These functions are not private but are for optimization_guide component
  // internal use only.

  // Called when browsing data is cleared for the user.
  void OnBrowsingDataRemoved();

  // Getter for the hint manager.
  optimization_guide::HintsManager* GetHintsManager();

  // Getter for the prediction manager.
  optimization_guide::PredictionManager* GetPredictionManager();

  // Getter for the optimization guide logger.
  OptimizationGuideLogger* GetOptimizationGuideLogger() {
    return optimization_guide_logger_.get();
  }

  // Adds hints for a URL with provided metadata to the optimization guide. For
  // testing purposes only. This will flush any callbacks for `url` that were
  // registered via `CanApplyOptimization`. If no applicable callbacks were
  // registered, this will just add the hint for later use.
  void AddHintForTesting(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      const std::optional<optimization_guide::OptimizationMetadata>& metadata);

 private:
  friend class OptimizationGuideServiceTest;
  friend class OptimizationGuideTabHelper;
  friend class OptimizationGuideTestAppInterfaceWrapper;

  // Notifies `hints_manager_` that the navigation associated with
  // `navigation_data` has started or redirected.
  void OnNavigationStartOrRedirect(
      OptimizationGuideNavigationData* navigation_data);

  // Notifies `hints_manager_` that the navigation associated with
  // `navigation_redirect_chain` has finished.
  void OnNavigationFinish(const std::vector<GURL>& navigation_redirect_chain);

  // KeyedService implementation:
  void Shutdown() override;

  // optimization_guide::OptimizationGuideDecider implementation:
  void CanApplyOptimizationOnDemand(
      const std::vector<GURL>& urls,
      const base::flat_set<optimization_guide::proto::OptimizationType>&
          optimization_types,
      optimization_guide::proto::RequestContext request_context,
      optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback
          callback,
      std::optional<optimization_guide::proto::RequestContextMetadata>
          request_context_metadata) override;

  // The store of hints.
  std::unique_ptr<optimization_guide::OptimizationGuideStore> hint_store_;

  // Manages the storing, loading, and fetching of hints.
  std::unique_ptr<optimization_guide::HintsManager> hints_manager_;

  // The top host provider to use for fetching information for the user's top
  // hosts. Will be null if the user has not consented to this type of browser
  // behavior.
  std::unique_ptr<optimization_guide::TopHostProvider> top_host_provider_;

  // The tab URL provider to use for fetching information for the user's active
  // tabs. Will be null if the user is off the record.
  std::unique_ptr<optimization_guide::TabUrlProvider> tab_url_provider_;

  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  // Manages the storing, loading, and evaluating of optimization target
  // prediction models.
  std::unique_ptr<optimization_guide::PredictionManager> prediction_manager_;

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  // Manages the state of the on-device model.
  scoped_refptr<optimization_guide::OnDeviceModelComponentStateManager>
      on_device_model_state_manager_;

  // Manages the model execution. Not created for off the record profiles.
  std::unique_ptr<optimization_guide::ModelExecutionManager>
      model_execution_manager_;
#endif

  // The PrefService of the profile this service is linked to.
  const raw_ptr<PrefService> pref_service_ = nullptr;

  // Whether the service is linked to an incognito profile.
  const bool off_the_record_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_SERVICE_H_
