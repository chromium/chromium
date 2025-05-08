// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_MODEL_ENHANCED_CALENDAR_SERVICE_IMPL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_MODEL_ENHANCED_CALENDAR_SERVICE_IMPL_H_

#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/optimization_guide/mojom/enhanced_calendar_service.mojom.h"
#import "mojo/public/cpp/bindings/receiver.h"

class OptimizationGuideService;

namespace optimization_guide {
class ModelQualityLogEntry;
struct OptimizationGuideModelExecutionResult;

namespace proto {
class PageContext;
class EnhancedCalendarRequest;
}  // namespace proto
}  // namespace optimization_guide

@class PageContextWrapper;

namespace web {
class WebState;
}  // namespace web

namespace ai {

class EnhancedCalendarServiceImpl : public mojom::EnhancedCalendarService,
                                    signin::IdentityManager::Observer {
 public:
  explicit EnhancedCalendarServiceImpl(
      mojo::PendingReceiver<mojom::EnhancedCalendarService> receiver,
      web::WebState* web_state);
  ~EnhancedCalendarServiceImpl() override;
  EnhancedCalendarServiceImpl(const EnhancedCalendarServiceImpl&) = delete;
  EnhancedCalendarServiceImpl& operator=(const EnhancedCalendarServiceImpl&) =
      delete;

  // ai::mojom::EnhancedCalendarServiceImpl:
  void ExecuteEnhancedCalendarRequest(
      mojom::EnhancedCalendarServiceRequestParamsPtr request_params,
      ExecuteEnhancedCalendarRequestCallback request_callback) override;

  // signin::IdentityManager::Observer implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* /*unused*/ identity_manager) override;

 private:
  // Handles the generated PageContext proto and executes the Enhanced Calendar
  // request.
  void OnPageContextGenerated(
      optimization_guide::proto::EnhancedCalendarRequest request,
      std::unique_ptr<optimization_guide::proto::PageContext> page_context);

  // Handles the Enhanced Calendar response (calls `request_callback` with the
  // response proto or error).
  void OnEnhancedCalendarResponse(
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> entry);

  // Invokes the pending callback.
  void InvokePendingCallback(
      mojom::EnhancedCalendarResponseResultPtr union_result);

  // Records response status in UMA.
  void RecordMetrics(std::optional<std::string> error_message);

  // Optimization Guide service to execute genAI queries.
  const raw_ref<OptimizationGuideService> service_;

  // Receiver throughout the EnhancedCalendarService lifecycle.
  mojo::Receiver<mojom::EnhancedCalendarService> receiver_;

  // Weak WebState.
  base::WeakPtr<web::WebState> web_state_;

  // The service's PageContext wrapper.
  PageContextWrapper* page_context_wrapper_;

  // Observer for `IdentityManager`.
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  // Callback for the current request, cleared once invoked.
  ExecuteEnhancedCalendarRequestCallback pending_request_callback_;

  // The identity manager that this instance uses.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // Weak pointer factory.
  base::WeakPtrFactory<EnhancedCalendarServiceImpl> weak_ptr_factory_{this};
};

}  // namespace ai

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_MODEL_ENHANCED_CALENDAR_SERVICE_IMPL_H_
