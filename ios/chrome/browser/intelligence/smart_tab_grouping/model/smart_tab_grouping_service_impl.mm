// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/smart_tab_grouping/model/smart_tab_grouping_service_impl.h"

#import <utility>

#import "base/functional/bind.h"
#import "base/time/time.h"
#import "components/optimization_guide/core/model_execution/feature_keys.h"
#import "components/optimization_guide/core/optimization_guide_util.h"
#import "components/optimization_guide/proto/features/ios_smart_tab_grouping.pb.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/ios_smart_tab_grouping_request_wrapper.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/web/public/web_state.h"
#import "mojo/public/cpp/base/proto_wrapper.h"

// TODO(crbug.com/459432095): Evaluate and set an appropriate timeout for Smart
// Tab Grouping.
const base::TimeDelta kSmartTabGroupingRequestTimeout = base::Seconds(15);

namespace ai {

SmartTabGroupingServiceImpl::SmartTabGroupingServiceImpl(
    mojo::PendingReceiver<ai::mojom::SmartTabGroupingService> receiver,
    WebStateList* web_state_list,
    PersistTabContextBrowserAgent* persist_tab_context_browser_agent)
    : optimization_guide_service_(
          *OptimizationGuideServiceFactory::GetForProfile(
              ProfileIOS::FromBrowserState(
                  web_state_list->GetActiveWebState()->GetBrowserState()))),
      smart_tab_grouping_receiver_(this, std::move(receiver)),
      web_state_list_(*web_state_list),
      persist_tab_context_browser_agent_(persist_tab_context_browser_agent),
      identity_manager_(
          *IdentityManagerFactory::GetForProfile(ProfileIOS::FromBrowserState(
              web_state_list->GetActiveWebState()->GetBrowserState()))) {
  identity_manager_observation_.Observe(&identity_manager_.get());
}

SmartTabGroupingServiceImpl::~SmartTabGroupingServiceImpl() {
  if (identity_manager_observation_.IsObserving()) {
    identity_manager_observation_.Reset();
  }

  // Cancel any in-flight requests and clean up.
  CancelPendingRequest("Service shutting down");
}

void SmartTabGroupingServiceImpl::ExecuteSmartTabGroupingRequest(
    ExecuteSmartTabGroupingRequestCallback request_callback) {
  if (pending_request_callback_) {
    CancelPendingRequest(
        "Request superseded by a new smart tab grouping request.");
  }
  pending_request_callback_ = std::move(request_callback);

  if (web_state_list_->empty()) {
    CancelPendingRequest("WebStateList empty");
    return;
  }

  if (smart_tab_grouping_request_wrapper_) {
    smart_tab_grouping_request_wrapper_ = nil;
  }

  base::OnceCallback<void(
      std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>)>
      wrapper_completion_callback = base::BindOnce(
          &SmartTabGroupingServiceImpl::OnRequestWrapperCompleted,
          weak_ptr_factory_.GetWeakPtr());

  // Instantiate and start the new wrapper to build the proto asynchronously.
  smart_tab_grouping_request_wrapper_ =
      [[IosSmartTabGroupingRequestWrapper alloc]
                   initWithWebStateList:&web_state_list_.get()
          persistTabContextBrowserAgent:persist_tab_context_browser_agent_
                     completionCallback:std::move(wrapper_completion_callback)];

  if (persist_tab_context_browser_agent_) {
    [smart_tab_grouping_request_wrapper_
        populateRequestFieldsAsyncFromPersistence];
  } else {
    [smart_tab_grouping_request_wrapper_
        populateRequestFieldsAsyncFromWebStates];
  }
}

void SmartTabGroupingServiceImpl::OnRequestWrapperCompleted(
    std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>
        request) {
  smart_tab_grouping_request_wrapper_ = nil;

  if (!request) {
    InvokePendingCallback(ai::mojom::SmartTabGroupingResponseResult::NewError(
        "Failed to populate request proto"));
    return;
  }

  optimization_guide_service_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kIosSmartTabGrouping,
      std::move(*request),
      {.execution_timeout = kSmartTabGroupingRequestTimeout},
      base::BindOnce(&SmartTabGroupingServiceImpl::OnSmartTabGroupingResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SmartTabGroupingServiceImpl::OnSmartTabGroupingResponse(
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> entry) {
  if (!pending_request_callback_) {
    return;
  }

  ai::mojom::SmartTabGroupingResponseResultPtr result_union;

  if (result.response.has_value()) {
    std::optional<optimization_guide::proto::IosSmartTabGroupingResponse>
        response_proto = optimization_guide::ParsedAnyMetadata<
            optimization_guide::proto::IosSmartTabGroupingResponse>(
            result.response.value());

    if (response_proto.has_value()) {
      result_union = ai::mojom::SmartTabGroupingResponseResult::NewResponse(
          mojo_base::ProtoWrapper(response_proto.value()));
    } else {
      result_union = ai::mojom::SmartTabGroupingResponseResult::NewError(
          "Proto unmarshalling error");
    }
  } else {
    std::string error_string =
        base::StrCat({"Server Model Execution Error: ",
                      optimization_guide_service_->ResponseForErrorCode(
                          static_cast<int>(result.response.error().error()))});
    result_union =
        ai::mojom::SmartTabGroupingResponseResult::NewError(error_string);
  }

  InvokePendingCallback(std::move(result_union));
}

void SmartTabGroupingServiceImpl::InvokePendingCallback(
    ai::mojom::SmartTabGroupingResponseResultPtr result_union) {
  CHECK(pending_request_callback_);
  std::move(pending_request_callback_).Run(std::move(result_union));
}

void SmartTabGroupingServiceImpl::CancelPendingRequest(
    const std::string& error_message) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  smart_tab_grouping_request_wrapper_ = nil;

  if (!pending_request_callback_) {
    return;
  }

  // Inform the caller of the error.
  ai::mojom::SmartTabGroupingResponseResultPtr result_union =
      ai::mojom::SmartTabGroupingResponseResult::NewError(error_message);
  InvokePendingCallback(std::move(result_union));
}

#pragma mark - IdentityManagerObserverBridgeDelegate

void SmartTabGroupingServiceImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  CancelPendingRequest("Primary account was changed.");
}

void SmartTabGroupingServiceImpl::OnIdentityManagerShutdown(
    signin::IdentityManager* /*unused*/ identity_manager) {
  identity_manager_observation_.Reset();
  CancelPendingRequest("IdentityManager shutdown.");
}

}  // namespace ai
