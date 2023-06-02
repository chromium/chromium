// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "ios/chrome/browser/download/background_service/background_download_service_factory.h"
#include "url/gurl.h"

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace optimization_guide {
class TabUrlProvider;
class TopHostProvider;
class OptimizationGuideStore;
class OptimizationTargetModelObserver;
class PredictionManager;
class HintsManager;
}  // namespace optimization_guide

class BrowserList;
class OptimizationGuideLogger;
class OptimizationGuideNavigationData;
class PrefService;

namespace web {
class BrowserState;
class NavigationContext;
}  // namespace web

// A BrowserState keyed service that is used to own the underlying Optimization
// Guide components. This is a rough copy of the OptimizationGuideKeyedService
// in //chrome/browser that is used for non-iOS. It cannot be directly used due
// to the platform differences of the common data structures -
// NavigationContext vs NavigationHandle, BrowserState vs Profile, etc.
// TODO(crbug.com/1240907): Add support for clearing the hints when browsing
// data is cleared.
class OptimizationGuideService
    : public KeyedService,
      public optimization_guide::NewOptimizationGuideDecider,
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
      base::WeakPtr<optimization_guide::OptimizationGuideStore>
          prediction_model_and_features_store,
      PrefService* pref_service,
      BrowserList* browser_list,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      BackgroundDownloadServiceProvider background_download_service_provider);
  ~OptimizationGuideService() override;

  OptimizationGuideService(const OptimizationGuideService&) = delete;
  OptimizationGuideService& operator=(const OptimizationGuideService&) = delete;

  // Some initialization parts must be done once the browser_state is fully
  // initialized.
  void DoFinalInit(download::BackgroundDownloadService*
                       background_download_service = nullptr);

  // Registers the optimization types that intend to be queried during the
  // session. It is expected for this to be called right after the browser has
  // been initialized.
  void RegisterOptimizationTypes(
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types) override;

  // optimization_guide::NewOptimizationGuideDecider implementation:
  // WARNING: This API is not quite ready for general use. Use
  // CanApplyOptimizationAsync or CanApplyOptimization using NavigationHandle
  // instead.
  void CanApplyOptimization(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationGuideDecisionCallback callback) override;

  // Returns whether `optimization_type` can be applied for `url`. This should
  // only be called for main frame navigations or future main frame navigations.
  optimization_guide::OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationMetadata* optimization_metadata) override;

  // Invokes `callback` with the decision for the URL contained in
  // `navigation_context` and `optimization_type`, when sufficient information
  // has been collected to make the decision. This should only be called for
  // main frame navigations.
  void CanApplyOptimizationAsync(
      web::NavigationContext* navigation_context,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationGuideDecisionCallback callback);

  // optimization_guide::OptimizationGuideModelProvider implementation
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const absl::optional<optimization_guide::proto::Any>& model_metadata,
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

  // optimization_guide::NewOptimizationGuideDecider implementation:
  void CanApplyOptimizationOnDemand(
      const std::vector<GURL>& urls,
      const base::flat_set<optimization_guide::proto::OptimizationType>&
          optimization_types,
      optimization_guide::proto::RequestContext request_context,
      optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback
          callback) override;

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

  std::unique_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  // The store of optimization target prediction models and features.
  std::unique_ptr<optimization_guide::OptimizationGuideStore>
      prediction_model_and_features_store_;

  // Manages the storing, loading, and evaluating of optimization target
  // prediction models.
  std::unique_ptr<optimization_guide::PredictionManager> prediction_manager_;

  // The PrefService of the browser state this service is linked to.
  PrefService* const pref_service_ = nullptr;

  // Whether the service is linked to an incognito browser state.
  const bool off_the_record_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_H_
