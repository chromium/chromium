// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/enhanced_calendar/model/enhanced_calendar_service_impl.h"

#import <optional>
#import <utility>

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/optimization_guide/core/model_execution/feature_keys.h"
#import "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#import "components/optimization_guide/core/optimization_guide_model_executor.h"
#import "components/optimization_guide/core/optimization_guide_util.h"
#import "components/optimization_guide/proto/features/enhanced_calendar.pb.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/web_state.h"
#import "mojo/public/cpp/base/proto_wrapper.h"

namespace ai {

// TODO(crbug.com/398903376): Evaluate a good timeout when using prod-served
// models.
const base::TimeDelta kEnhancedCalendarRequestTimeout = base::Seconds(15);

// TODO(crbug.com/399895707): Observe for profile changes and handle them for
// the `OptimizationGuideService`.
EnhancedCalendarServiceImpl::EnhancedCalendarServiceImpl(
    mojo::PendingReceiver<mojom::EnhancedCalendarService> receiver,
    web::WebState* web_state)
    : service_(*OptimizationGuideServiceFactory::GetForProfile(
          ProfileIOS::FromBrowserState(web_state->GetBrowserState()))),
      receiver_(this, std::move(receiver)) {
  web_state_ = web_state->GetWeakPtr();
}

EnhancedCalendarServiceImpl::~EnhancedCalendarServiceImpl() = default;

void EnhancedCalendarServiceImpl::ExecuteEnhancedCalendarRequest(
    mojom::EnhancedCalendarServiceRequestParamsPtr request_params,
    ExecuteEnhancedCalendarRequestCallback request_callback) {
  if (!web_state_) {
    mojom::EnhancedCalendarResponseResultPtr result_union =
        mojom::EnhancedCalendarResponseResult::NewError(
            "WebState destroyed before executing request");
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
                     weak_ptr_factory_.GetWeakPtr(), std::move(request),
                     std::move(request_callback));

  // Populate the PageContext proto and then execute the query.
  page_context_wrapper_ = [[PageContextWrapper alloc]
        initWithWebState:web_state_.get()
      completionCallback:std::move(page_context_completion_callback)];
  [page_context_wrapper_ setShouldGetInnerText:YES];
  [page_context_wrapper_ setShouldGetSnapshot:YES];
  [page_context_wrapper_
      setTextToHighlight:base::SysUTF8ToNSString(
                             request_params->surrounding_text)];
  [page_context_wrapper_ populatePageContextFieldsAsync];
}

#pragma mark - Private

// Execute the Enhanced Calendar request with the generated Page Context.
void EnhancedCalendarServiceImpl::OnPageContextGenerated(
    optimization_guide::proto::EnhancedCalendarRequest request,
    ExecuteEnhancedCalendarRequestCallback request_callback,
    std::unique_ptr<optimization_guide::proto::PageContext> page_context) {
  page_context_wrapper_ = nil;
  request.set_allocated_page_context(page_context.release());

  service_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kEnhancedCalendar, request,
      kEnhancedCalendarRequestTimeout,
      base::BindOnce(&EnhancedCalendarServiceImpl::OnEnhancedCalendarResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(request_callback)));
}

void EnhancedCalendarServiceImpl::OnEnhancedCalendarResponse(
    ExecuteEnhancedCalendarRequestCallback request_callback,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> entry) {
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
          "Proto unmarshalling error.");
    }
  } else {
    std::string error_string =
        base::StrCat({"Server model execution error: ",
                      service_->ResponseForErrorCode(
                          static_cast<int>(result.response.error().error()))});
    result_union =
        mojom::EnhancedCalendarResponseResult::NewError(error_string);
  }

  std::move(request_callback).Run(std::move(result_union));
}

}  // namespace ai
