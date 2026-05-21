// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ZERO_STATE_SUGGESTIONS_MODEL_MODEL_LED_SUGGESTIONS_SERVICE_IMPL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ZERO_STATE_SUGGESTIONS_MODEL_MODEL_LED_SUGGESTIONS_SERVICE_IMPL_H_

#import "base/memory/weak_ptr.h"
#import "base/types/expected.h"
#import "ios/chrome/browser/optimization_guide/mojom/model_led_suggestions_service.mojom.h"
#import "mojo/public/cpp/bindings/receiver.h"
#import "url/gurl.h"

namespace web {
class WebState;
}  // namespace web

class OptimizationGuideService;

namespace optimization_guide {
struct OptimizationGuideModelExecutionResult;
class ModelQualityLogEntry;
namespace proto {
class PageContext;
class ZeroStateSuggestionsRequest;
}  // namespace proto
}  // namespace optimization_guide

enum class PageContextWrapperError;

@class PageContextWrapper;

namespace ai {

class ModelLedSuggestionsServiceImpl
    : public mojom::ModelLedSuggestionsService {
 public:
  explicit ModelLedSuggestionsServiceImpl(
      mojo::PendingReceiver<mojom::ModelLedSuggestionsService> receiver,
      web::WebState* web_state);
  ~ModelLedSuggestionsServiceImpl() override;
  ModelLedSuggestionsServiceImpl(const ModelLedSuggestionsServiceImpl&) =
      delete;
  ModelLedSuggestionsServiceImpl& operator=(
      const ModelLedSuggestionsServiceImpl&) = delete;

  // ai::mojom::ModelLedSuggestionsService:
  void FetchModelLedSuggestions(
      FetchModelLedSuggestionsCallback callback) override;

 private:
  // Handles the generated PageContext proto and executes the request.
  void OnPageContextGenerated(
      optimization_guide::proto::ZeroStateSuggestionsRequest request,
      base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                     PageContextWrapperError> response);

  // Handles the response for a model-led suggestions query.
  void OnModelLedSuggestionsResponse(
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> entry);

  // Cancels any in-flight requests.
  void CancelOngoingRequests();

  // Service used to execute LLM queries.
  raw_ptr<OptimizationGuideService> service_;

  // Receiver throughout the ModelLedSuggestionsServiceImpl lifecycle.
  mojo::Receiver<mojom::ModelLedSuggestionsService> receiver_;

  // Weak WebState.
  base::WeakPtr<web::WebState> web_state_;

  // The service's PageContext wrapper.
  PageContextWrapper* page_context_wrapper_;

  // Callback for the current request, cleared once invoked.
  FetchModelLedSuggestionsCallback pending_request_callback_;

  // The URL of the ongoing request.
  GURL model_led_suggestions_url_;

  // Weak pointer factory.
  base::WeakPtrFactory<ModelLedSuggestionsServiceImpl> weak_ptr_factory_{this};
};

}  // namespace ai
#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ZERO_STATE_SUGGESTIONS_MODEL_MODEL_LED_SUGGESTIONS_SERVICE_IMPL_H_
