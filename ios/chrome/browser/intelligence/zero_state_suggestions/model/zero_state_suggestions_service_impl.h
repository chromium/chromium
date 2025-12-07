// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ZERO_STATE_SUGGESTIONS_MODEL_ZERO_STATE_SUGGESTIONS_SERVICE_IMPL_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ZERO_STATE_SUGGESTIONS_MODEL_ZERO_STATE_SUGGESTIONS_SERVICE_IMPL_H_

#import "base/memory/weak_ptr.h"
#import "base/types/expected.h"
#import "ios/chrome/browser/optimization_guide/mojom/zero_state_suggestions_service.mojom.h"
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

class ZeroStateSuggestionsServiceImpl
    : public mojom::ZeroStateSuggestionsService {
 public:
  explicit ZeroStateSuggestionsServiceImpl(
      mojo::PendingReceiver<mojom::ZeroStateSuggestionsService> receiver,
      web::WebState* web_state);
  ~ZeroStateSuggestionsServiceImpl() override;
  ZeroStateSuggestionsServiceImpl(const ZeroStateSuggestionsServiceImpl&) =
      delete;
  ZeroStateSuggestionsServiceImpl& operator=(
      const ZeroStateSuggestionsServiceImpl&) = delete;

  // ai::mojom::ZeroStateSuggestionsServiceImpl:
  void FetchZeroStateSuggestions(
      FetchZeroStateSuggestionsCallback callback) override;

 private:
  // Handles the generated PageContext proto and executes the request.
  void OnPageContextGenerated(
      optimization_guide::proto::ZeroStateSuggestionsRequest request,
      base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                     PageContextWrapperError> response);

  // Handles the response for a zero state suggestions query.
  void OnZeroStateSuggestionsResponse(
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> entry);

  // Cancels any in-flight requests.
  void CancelOngoingRequests();

  // Service used to execute LLM queries.
  raw_ptr<OptimizationGuideService> service_;

  // Receiver throughout the ZeroStateSuggestionsServiceImpl lifecycle.
  mojo::Receiver<mojom::ZeroStateSuggestionsService> receiver_;

  // Weak WebState.
  base::WeakPtr<web::WebState> web_state_;

  // The service's PageContext wrapper.
  PageContextWrapper* page_context_wrapper_;

  // Callback for the current request, cleared once invoked.
  FetchZeroStateSuggestionsCallback pending_request_callback_;

  // The URL of the ongoing request.
  GURL zero_state_suggestions_url_;

  // Weak pointer factory.
  base::WeakPtrFactory<ZeroStateSuggestionsServiceImpl> weak_ptr_factory_{this};
};

}  // namespace ai
#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ZERO_STATE_SUGGESTIONS_MODEL_ZERO_STATE_SUGGESTIONS_SERVICE_IMPL_H_
