// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/enhanced_calendar/model/enhanced_calendar_service_impl.h"

#import <optional>
#import <string>
#import <utility>

#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/optimization_guide/core/model_execution/feature_keys.h"
#import "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#import "components/optimization_guide/core/optimization_guide_model_executor.h"
#import "components/optimization_guide/core/optimization_guide_util.h"
#import "components/optimization_guide/proto/features/enhanced_calendar.pb.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/constants/error_strings.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/metrics/enhanced_calendar_metrics.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/web/public/web_state.h"
#import "mojo/public/cpp/base/proto_wrapper.h"

namespace ai {

// TODO(crbug.com/398903376): Evaluate a good timeout when using prod-served
// models.
const base::TimeDelta kEnhancedCalendarRequestTimeout = base::Seconds(15);

EnhancedCalendarServiceImpl::EnhancedCalendarServiceImpl(
    mojo::PendingReceiver<mojom::EnhancedCalendarService> receiver,
    web::WebState* web_state)
    : service_(*OptimizationGuideServiceFactory::GetForProfile(
          ProfileIOS::FromBrowserState(web_state->GetBrowserState()))),
      receiver_(this, std::move(receiver)),
      identity_manager_(IdentityManagerFactory::GetForProfile(
          ProfileIOS::FromBrowserState(web_state->GetBrowserState()))) {
  web_state_ = web_state->GetWeakPtr();
  CHECK(identity_manager_);
  identity_manager_observation_.Observe(identity_manager_);
}

EnhancedCalendarServiceImpl::~EnhancedCalendarServiceImpl() {
  if (identity_manager_observation_.IsObserving()) {
    identity_manager_observation_.Reset();
  }

  // Cancel any in-flight requests.
  weak_ptr_factory_.InvalidateWeakPtrs();
  page_context_wrapper_ = nil;

  if (!pending_request_callback_) {
    return;
  }

  mojom::EnhancedCalendarResponseResultPtr result_union =
      mojom::EnhancedCalendarResponseResult::NewError(
          GetEnhancedCalendarErrorString(
              EnhancedCalendarError::kServiceShuttingDownError));

  InvokePendingCallback(std::move(result_union));
}

void EnhancedCalendarServiceImpl::ExecuteEnhancedCalendarRequest(
    mojom::EnhancedCalendarServiceRequestParamsPtr request_params,
    ExecuteEnhancedCalendarRequestCallback request_callback) {
  // Store the callback for the current request.
  pending_request_callback_ = std::move(request_callback);

  if (!web_state_) {
    mojom::EnhancedCalendarResponseResultPtr result_union =
        mojom::EnhancedCalendarResponseResult::NewError(
            GetEnhancedCalendarErrorString(
                EnhancedCalendarError::kWebStateDestroyedBeforeRequestError));

    std::move(request_callback).Run(std::move(result_union));
    return;
  }

  // Create the request, and set the selected and surrounding texts on it.
  optimization_guide::proto::EnhancedCalendarRequest request;
  request.set_selected_text(request_params->selected_text);
  request.set_surrounding_text(request_params->surrounding_text);

  // Set the prompt, if it exists.
  if (request_params->optional_prompt.has_value() &&
      !request_params->optional_prompt.value().empty()) {
    request.set_prompt(request_params->optional_prompt.value());
  }

  // Callback to execute when the PageContext proto is done being generated.
  auto page_context_completion_callback =
      base::BindOnce(&EnhancedCalendarServiceImpl::OnPageContextGenerated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request));

  // Populate the PageContext proto and then execute the query.
  page_context_wrapper_ = [[PageContextWrapper alloc]
        initWithWebState:web_state_.get()
      completionCallback:std::move(page_context_completion_callback)];
  [page_context_wrapper_ setShouldGetInnerText:YES];
  [page_context_wrapper_ setShouldGetSnapshot:YES];
  [page_context_wrapper_ setTextToHighlight:base::SysUTF8ToNSString(
                                                request_params->selected_text)];
  [page_context_wrapper_ populatePageContextFieldsAsync];
}

#pragma mark - Private

// Execute the Enhanced Calendar request with the generated Page Context.
void EnhancedCalendarServiceImpl::OnPageContextGenerated(
    optimization_guide::proto::EnhancedCalendarRequest request,
    std::unique_ptr<optimization_guide::proto::PageContext> page_context) {
  page_context_wrapper_ = nil;

  // The request might have been cancelled.
  if (!pending_request_callback_) {
    return;
  }

  request.set_allocated_page_context(page_context.release());

  service_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kEnhancedCalendar, request,
      kEnhancedCalendarRequestTimeout,
      base::BindOnce(&EnhancedCalendarServiceImpl::OnEnhancedCalendarResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnhancedCalendarServiceImpl::OnEnhancedCalendarResponse(
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> entry) {
  // The request might have been cancelled.
  if (!pending_request_callback_) {
    return;
  }

  mojom::EnhancedCalendarResponseResultPtr result_union;

  if (result.response.has_value()) {
    std::optional<optimization_guide::proto::EnhancedCalendarResponse>
        response_proto = optimization_guide::ParsedAnyMetadata<
            optimization_guide::proto::EnhancedCalendarResponse>(
            result.response.value());

    if (response_proto.has_value()) {
      result_union = mojom::EnhancedCalendarResponseResult::NewResponse(
          mojo_base::ProtoWrapper(response_proto.value()));
    } else {
      result_union = mojom::EnhancedCalendarResponseResult::NewError(
          GetEnhancedCalendarErrorString(
              EnhancedCalendarError::kProtoUnmarshallingError));
    }
  } else {
    std::string error_string =
        base::StrCat({GetEnhancedCalendarErrorString(
                          EnhancedCalendarError::kServerModelExecutionError),
                      service_->ResponseForErrorCode(
                          static_cast<int>(result.response.error().error()))});
    result_union =
        mojom::EnhancedCalendarResponseResult::NewError(error_string);
  }

  InvokePendingCallback(std::move(result_union));
}

void EnhancedCalendarServiceImpl::InvokePendingCallback(
    mojom::EnhancedCalendarResponseResultPtr result_union) {
  CHECK(pending_request_callback_);
  RecordMetrics(result_union->is_error()
                    ? std::make_optional(result_union->get_error())
                    : std::nullopt);

  std::move(pending_request_callback_).Run(std::move(result_union));
}

void EnhancedCalendarServiceImpl::RecordMetrics(
    std::optional<std::string> error_message) {
  if (!error_message.has_value()) {
    base::UmaHistogramEnumeration(kEnhancedCalendarResponseStatusHistogram,
                                  EnhancedCalendarResponseStatus::kSuccess);
    return;
  }

  if (error_message.value() ==
      GetEnhancedCalendarErrorString(
          EnhancedCalendarError::kServiceShuttingDownError)) {
    base::UmaHistogramEnumeration(
        kEnhancedCalendarResponseStatusHistogram,
        EnhancedCalendarResponseStatus::kCancelRequest);
    return;
  }

  base::UmaHistogramEnumeration(
      kEnhancedCalendarResponseStatusHistogram,
      EnhancedCalendarResponseStatus::kGenericFailure);
}

#pragma mark - IdentityManagerObserverBridgeDelegate

void EnhancedCalendarServiceImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  // Cancel the original callback by invalidating weak pointers and instead
  // invoke the response handler with an error message 'Primary account was
  // changed'.
  if (!pending_request_callback_) {
    return;
  }

  weak_ptr_factory_.InvalidateWeakPtrs();
  mojom::EnhancedCalendarResponseResultPtr result_union =
      mojom::EnhancedCalendarResponseResult::NewError(
          GetEnhancedCalendarErrorString(
              EnhancedCalendarError::kPrimaryAccountChangeError));

  InvokePendingCallback(std::move(result_union));
}

void EnhancedCalendarServiceImpl::OnIdentityManagerShutdown(
    signin::IdentityManager* /*unused*/ identity_manager) {
  identity_manager_observation_.Reset();
}

}  // namespace ai
