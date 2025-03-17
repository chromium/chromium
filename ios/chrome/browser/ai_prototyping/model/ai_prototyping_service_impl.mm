// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/model/ai_prototyping_service_impl.h"

#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

#import "components/optimization_guide/proto/features/bling_prototyping.pb.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/optimization_guide/proto/features/tab_organization.pb.h"
#import "components/optimization_guide/proto/string_value.pb.h"  // nogncheck

namespace ai {

AIPrototypingServiceImpl::AIPrototypingServiceImpl(
    mojo::PendingReceiver<mojom::AIPrototypingService> receiver,
    web::BrowserState* browser_state,
    bool start_on_device)
    : receiver_(this, std::move(receiver)) {
  service_ = OptimizationGuideServiceFactory::GetForProfile(
      ProfileIOS::FromBrowserState(browser_state));
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  if (start_on_device) {
    StartOnDeviceSession();
  }
#endif
}

AIPrototypingServiceImpl::~AIPrototypingServiceImpl() = default;

void AIPrototypingServiceImpl::ExecuteServerQuery(
    ::mojo_base::ProtoWrapper request,
    ExecuteServerQueryCallback callback) {
  optimization_guide::proto::BlingPrototypingRequest proto_request =
      request.As<optimization_guide::proto::BlingPrototypingRequest>().value();

  optimization_guide::OptimizationGuideModelExecutionResultCallback
      result_callback = base::BindOnce(
          [](ExecuteServerQueryCallback query_callback,
             AIPrototypingServiceImpl* service,
             optimization_guide::OptimizationGuideModelExecutionResult result,
             std::unique_ptr<optimization_guide::ModelQualityLogEntry> entry) {
            std::string response =
                service->OnServerModelExecuteResponse(std::move(result));
            std::move(query_callback).Run(response);
          },
          std::move(callback), base::Unretained(this));

  service_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kBlingPrototyping,
      proto_request,
      /*execution_timeout*/ std::nullopt, std::move(result_callback));
}

void AIPrototypingServiceImpl::ExecuteOnDeviceQuery(
    ::mojo_base::ProtoWrapper request,
    ExecuteOnDeviceQueryCallback callback) {
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  if (!on_device_session_) {
    std::move(callback).Run("Session is not ready for querying yet.");
    StartOnDeviceSession();
    return;
  }

  optimization_guide::proto::StringValue proto_request =
      request.As<optimization_guide::proto::StringValue>().value();

  optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
      result_callback = base::RepeatingCallback(base::BindRepeating(
          [](ExecuteOnDeviceQueryCallback query_callback,
             AIPrototypingServiceImpl* service,
             optimization_guide::OptimizationGuideModelStreamingExecutionResult
                 result) {
            std::string response =
                service->OnDeviceModelExecuteResponse(std::move(result));
            std::move(query_callback).Run(response);
          },
          base::Passed(std::move(callback)), base::Unretained(this)));

  on_device_session_->ExecuteModel(proto_request, std::move(result_callback));
#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
}

std::string AIPrototypingServiceImpl::OnServerModelExecuteResponse(
    optimization_guide::OptimizationGuideModelExecutionResult result) {
  std::string response = "";

  if (result.response.has_value()) {
    auto parsed = optimization_guide::ParsedAnyMetadata<
        optimization_guide::proto::BlingPrototypingResponse>(
        result.response.value());
    if (!parsed->output().empty()) {
      response = parsed->output();
    } else {
      return "Empty server response.";
    }
  } else {
    return base::StrCat({"Server model execution error: ",
                         service_->ResponseForErrorCode(static_cast<int>(
                             result.response.error().error()))});
  }

  return response;
}

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
std::string AIPrototypingServiceImpl::OnDeviceModelExecuteResponse(
    optimization_guide::OptimizationGuideModelStreamingExecutionResult result) {
  std::string response = "";

  if (result.response.has_value()) {
    auto parsed = optimization_guide::ParsedAnyMetadata<
        optimization_guide::proto::StringValue>(result.response->response);
    if (parsed->has_value()) {
      response = parsed->value();
    } else {
      response = "Failed to parse device response as a string";
    }
    if (result.response->is_complete) {
      on_device_session_.reset();
    }
  } else {
    response =
        base::StringPrintf("On-device model execution error: %d",
                           static_cast<int>(result.response.error().error()));
  }

  return response;
}

void AIPrototypingServiceImpl::StartOnDeviceSession() {
  optimization_guide::SessionConfigParams configParams =
      optimization_guide::SessionConfigParams{
          .execution_mode = optimization_guide::SessionConfigParams::
              ExecutionMode::kOnDeviceOnly,
          .logging_mode = optimization_guide::SessionConfigParams::LoggingMode::
              kAlwaysDisable,
      };
  on_device_session_ = service_->StartSession(
      optimization_guide::ModelBasedCapabilityKey::kPromptApi, configParams);
}

#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

}  // namespace ai
