// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/zero_state_suggestions/model/zero_state_suggestions_service_impl.h"

#import "base/functional/bind.h"
#import "components/optimization_guide/core/model_execution/feature_keys.h"
#import "components/optimization_guide/core/optimization_guide_util.h"
#import "components/optimization_guide/proto/features/zero_state_suggestions.pb.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/web_state.h"

namespace {
// Timeout for the model execution.
const base::TimeDelta kModelExecutionTimeout = base::Seconds(5);

// Helper to chain two zero-state callbacks for same-page requests.
void RunChainedCallbacks(
    ai::mojom::ZeroStateSuggestionsService::FetchZeroStateSuggestionsCallback
        old_callback,
    ai::mojom::ZeroStateSuggestionsService::FetchZeroStateSuggestionsCallback
        new_callback,
    ai::mojom::ZeroStateSuggestionsResponseResultPtr result) {
  ai::mojom::ZeroStateSuggestionsResponseResultPtr result_for_new_callback;
  if (result->is_error()) {
    result_for_new_callback =
        ai::mojom::ZeroStateSuggestionsResponseResult::NewError(
            result->get_error());
  } else {
    std::optional<optimization_guide::proto::ZeroStateSuggestionsResponse>
        original_proto =
            result->get_response()
                .As<optimization_guide::proto::ZeroStateSuggestionsResponse>();
    if (original_proto) {
      auto cloned_proto = *original_proto;
      result_for_new_callback =
          ai::mojom::ZeroStateSuggestionsResponseResult::NewResponse(
              mojo_base::ProtoWrapper(std::move(cloned_proto)));
    } else {
      result_for_new_callback =
          ai::mojom::ZeroStateSuggestionsResponseResult::NewError(
              "Proto deserialization error.");
    }
  }
  std::move(old_callback).Run(std::move(result));
  std::move(new_callback).Run(std::move(result_for_new_callback));
}

}  // namespace

namespace ai {

ZeroStateSuggestionsServiceImpl::ZeroStateSuggestionsServiceImpl(
    mojo::PendingReceiver<mojom::ZeroStateSuggestionsService> receiver,
    web::WebState* web_state)
    : receiver_(this, std::move(receiver)) {
  web_state_ = web_state->GetWeakPtr();
  service_ = OptimizationGuideServiceFactory::GetForProfile(
      ProfileIOS::FromBrowserState(web_state->GetBrowserState()));
}

ZeroStateSuggestionsServiceImpl::~ZeroStateSuggestionsServiceImpl() {
  CancelOngoingRequests();
}

void ZeroStateSuggestionsServiceImpl::FetchZeroStateSuggestions(
    FetchZeroStateSuggestionsCallback callback) {
  if (!web_state_) {
    mojom::ZeroStateSuggestionsResponseResultPtr result_union =
        mojom::ZeroStateSuggestionsResponseResult::NewError(
            "WebState destroyed.");
    std::move(callback).Run(std::move(result_union));
    return;
  }

  const GURL current_page_url = web_state_->GetVisibleURL();

  // If there's an ongoing request for the same page, chain the new callback to
  // the existing one.
  if (pending_request_callback_ &&
      zero_state_suggestions_url_ == current_page_url) {
    pending_request_callback_ = base::BindOnce(
        &RunChainedCallbacks, std::move(pending_request_callback_),
        std::move(callback));
    return;
  }

  // Cancel any in-flight requests for a different page.
  CancelOngoingRequests();
  pending_request_callback_ = std::move(callback);
  zero_state_suggestions_url_ = current_page_url;

  optimization_guide::proto::ZeroStateSuggestionsRequest request;

  // Callback to execute when the PageContext proto is done being generated.
  auto page_context_completion_callback =
      base::BindOnce(&ZeroStateSuggestionsServiceImpl::OnPageContextGenerated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request));

  // Populate the PageContext proto and then execute the query.
  page_context_wrapper_ = [[PageContextWrapper alloc]
        initWithWebState:web_state_.get()
      completionCallback:std::move(page_context_completion_callback)];
  [page_context_wrapper_ setShouldGetInnerText:YES];
  [page_context_wrapper_ populatePageContextFieldsAsync];
}

void ZeroStateSuggestionsServiceImpl::OnPageContextGenerated(
    optimization_guide::proto::ZeroStateSuggestionsRequest request,
    base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                   PageContextWrapperError> response) {
  page_context_wrapper_ = nil;

  if (!pending_request_callback_ || !response.has_value()) {
    mojom::ZeroStateSuggestionsResponseResultPtr result_union =
        mojom::ZeroStateSuggestionsResponseResult::NewError(
            "Failed to generate PageContext.");
    std::move(pending_request_callback_).Run(std::move(result_union));
    return;
  }

  request.set_allocated_page_context(response.value().release());

  optimization_guide::OptimizationGuideModelExecutionResultCallback
      result_callback = base::BindOnce(
          &ZeroStateSuggestionsServiceImpl::OnZeroStateSuggestionsResponse,
          weak_ptr_factory_.GetWeakPtr());

  service_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kZeroStateSuggestions,
      request, {.execution_timeout = kModelExecutionTimeout},
      std::move(result_callback));
}

void ZeroStateSuggestionsServiceImpl::OnZeroStateSuggestionsResponse(
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> entry) {
  if (!pending_request_callback_) {
    return;
  }

  mojom::ZeroStateSuggestionsResponseResultPtr result_union;

  if (result.response.has_value()) {
    std::optional<optimization_guide::proto::ZeroStateSuggestionsResponse>
        response_proto = optimization_guide::ParsedAnyMetadata<
            optimization_guide::proto::ZeroStateSuggestionsResponse>(
            result.response.value());

    if (response_proto.has_value()) {
      result_union = mojom::ZeroStateSuggestionsResponseResult::NewResponse(
          mojo_base::ProtoWrapper(response_proto.value()));
    } else {
      result_union = mojom::ZeroStateSuggestionsResponseResult::NewError(
          "Proto unmarshalling error.");
    }
  } else {
    result_union = mojom::ZeroStateSuggestionsResponseResult::NewError(
        "Server model execution error.");
  }

  std::move(pending_request_callback_).Run(std::move(result_union));
}

void ZeroStateSuggestionsServiceImpl::CancelOngoingRequests() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  page_context_wrapper_ = nil;
  zero_state_suggestions_url_ = GURL();
  if (pending_request_callback_) {
    mojom::ZeroStateSuggestionsResponseResultPtr result_union =
        mojom::ZeroStateSuggestionsResponseResult::NewError(
            "Zero state suggestions request cancelled.");
    std::move(pending_request_callback_).Run(std::move(result_union));
  }
}

}  // namespace ai
