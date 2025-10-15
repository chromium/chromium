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
  // Cancel any in-flight requests.
  CancelOngoingRequests();
  pending_request_callback_ = std::move(callback);

  if (!web_state_) {
    mojom::ZeroStateSuggestionsResponseResultPtr result_union =
        mojom::ZeroStateSuggestionsResponseResult::NewError(
            "WebState destroyed.");
    std::move(pending_request_callback_).Run(std::move(result_union));
    return;
  }

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
      request, kModelExecutionTimeout, std::move(result_callback));
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
  pending_request_callback_.Reset();
}

}  // namespace ai
