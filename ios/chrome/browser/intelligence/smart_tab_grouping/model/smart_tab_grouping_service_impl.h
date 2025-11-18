// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_SMART_TAB_GROUPING_MODEL_SMART_TAB_GROUPING_SERVICE_IMPL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_SMART_TAB_GROUPING_MODEL_SMART_TAB_GROUPING_SERVICE_IMPL_H_

#import "base/scoped_observation.h"
#import "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/optimization_guide/mojom/smart_tab_grouping_service.mojom.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"
#import "mojo/public/cpp/bindings/receiver.h"

class OptimizationGuideService;
class WebStateList;
class PersistTabContextBrowserAgent;
@class IosSmartTabGroupingRequestWrapper;

namespace optimization_guide {
struct OptimizationGuideModelExecutionResult;
namespace proto {
class IosSmartTabGroupingRequest;
}  // namespace proto
}  // namespace optimization_guide

namespace ai {

// Implementation of ai::mojom::SmartTabGroupingService.
class SmartTabGroupingServiceImpl : public ai::mojom::SmartTabGroupingService,
                                    public signin::IdentityManager::Observer {
 public:
  SmartTabGroupingServiceImpl(
      mojo::PendingReceiver<ai::mojom::SmartTabGroupingService> receiver,
      WebStateList* web_state_list,
      PersistTabContextBrowserAgent* persist_tab_context_browser_agent);
  ~SmartTabGroupingServiceImpl() override;

  SmartTabGroupingServiceImpl(const SmartTabGroupingServiceImpl&) = delete;
  SmartTabGroupingServiceImpl& operator=(const SmartTabGroupingServiceImpl&) =
      delete;

  // ai::mojom::SmartTabGroupingService implementation:
  void ExecuteSmartTabGroupingRequest(
      ExecuteSmartTabGroupingRequestCallback request_callback) override;

  // signin::IdentityManager::Observer implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* /*unused*/ identity_manager) override;

 private:
  // Callback for the IosSmartTabGroupingRequestWrapper.
  void OnRequestWrapperCompleted(
      std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>
          request);

  // Callback for OptimizationGuideService::ExecuteModel.
  void OnSmartTabGroupingResponse(
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> entry);

  // Invokes the pending request callback with the result.
  void InvokePendingCallback(
      ai::mojom::SmartTabGroupingResponseResultPtr result_union);

  // Helper function to cancel pending operations and notify the caller.
  void CancelPendingRequest(const std::string& error_message);

  // Optimization Guide service to execute genAI queries.
  const raw_ref<OptimizationGuideService> optimization_guide_service_;

  // Receiver throughout the SmartTabGroupingService lifecycle.
  mojo::Receiver<ai::mojom::SmartTabGroupingService>
      smart_tab_grouping_receiver_;

  // Callback for the current request, cleared once invoked.
  ExecuteSmartTabGroupingRequestCallback pending_request_callback_;

  // The list of WebStates for the current browser.
  const raw_ref<WebStateList> web_state_list_;

  // The agent responsible for persisting and restoring tab context.
  raw_ptr<PersistTabContextBrowserAgent> persist_tab_context_browser_agent_;

  // Pointer to the Objective-C request wrapper.
  __strong IosSmartTabGroupingRequestWrapper*
      smart_tab_grouping_request_wrapper_ = nil;

  // Observer for `IdentityManager`.
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  // The identity manager that this instance uses.
  const raw_ref<signin::IdentityManager> identity_manager_;

  // Weak pointer factory.
  base::WeakPtrFactory<SmartTabGroupingServiceImpl> weak_ptr_factory_{this};
};

}  // namespace ai

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_SMART_TAB_GROUPING_MODEL_SMART_TAB_GROUPING_SERVICE_IMPL_H_
