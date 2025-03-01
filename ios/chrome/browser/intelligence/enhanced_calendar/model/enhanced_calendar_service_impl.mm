// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/enhanced_calendar/model/enhanced_calendar_service_impl.h"

#import <optional>
#import <utility>

#import "base/functional/bind.h"
#import "base/time/time.h"
#import "components/optimization_guide/core/model_execution/feature_keys.h"
#import "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#import "components/optimization_guide/core/optimization_guide_model_executor.h"
#import "components/optimization_guide/core/optimization_guide_util.h"
#import "components/optimization_guide/proto/features/enhanced_calendar.pb.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "mojo/public/cpp/base/proto_wrapper.h"

namespace ai {

// TODO(crbug.com/398903376): Evaluate a good timeout when using prod-served
// models.
const base::TimeDelta kEnhancedCalendarRequestTimeout = base::Seconds(15);

// TODO(crbug.com/399895707): Observe for profile changes and handle them for
// the `OptimizationGuideService`.
EnhancedCalendarServiceImpl::EnhancedCalendarServiceImpl(
    mojo::PendingReceiver<mojom::EnhancedCalendarService> receiver,
    web::BrowserState* browser_state)
    : service_(*OptimizationGuideServiceFactory::GetForProfile(
          ProfileIOS::FromBrowserState(browser_state))),
      receiver_(this, std::move(receiver)) {}

EnhancedCalendarServiceImpl::~EnhancedCalendarServiceImpl() = default;

void EnhancedCalendarServiceImpl::ExecuteEnhancedCalendarRequest(
    ::mojo_base::ProtoWrapper request,
    ExecuteEnhancedCalendarRequestCallback request_callback) {
  optimization_guide::proto::EnhancedCalendarRequest request_proto =
      request.As<optimization_guide::proto::EnhancedCalendarRequest>().value();

  service_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kEnhancedCalendar,
      request_proto, kEnhancedCalendarRequestTimeout,
      base::BindOnce(&EnhancedCalendarServiceImpl::OnEnhancedCalendarResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(request_callback)));
}

#pragma mark - Private

void EnhancedCalendarServiceImpl::OnEnhancedCalendarResponse(
    ExecuteEnhancedCalendarRequestCallback request_callback,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> entry) {
  mojom::EnhancedCalendarResponseResultPtr result_union;

  if (!result.response.has_value()) {
    std::string error_string =
        base::StrCat({"Server model execution error: ",
                      service_->ResponseForErrorCode(
                          static_cast<int>(result.response.error().error()))});
    result_union =
        mojom::EnhancedCalendarResponseResult::NewError(error_string);
  } else {
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
  }

  std::move(request_callback).Run(std::move(result_union));
}

}  // namespace ai
