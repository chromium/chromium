// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/coordinator/ai_prototyping_mediator.h"

#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "components/optimization_guide/optimization_guide_buildflags.h"
#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_consumer.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/web_state.h"

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
#import "components/optimization_guide/proto/features/bling_prototyping.pb.h"
#import "components/optimization_guide/proto/string_value.pb.h"  // nogncheck
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#endif

@implementation AIPrototypingMediator {
  // The web state that triggered the menu.
  base::WeakPtr<web::WebState> _webState;

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  // Service used to execute LLM queries.
  raw_ptr<OptimizationGuideService> _service;

  // Retains the on-device session in memory.
  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      _on_device_session;
#endif
}

- (instancetype)initWithWebState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    _webState = webState->GetWeakPtr();
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
    _service = OptimizationGuideServiceFactory::GetForProfile(
        ProfileIOS::FromBrowserState(_webState->GetBrowserState()));

    [self startOnDeviceSession];
#endif
  }
  return self;
}

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
#pragma mark - AIPrototypingMutator

- (void)executeServerQuery:
    (optimization_guide::proto::BlingPrototypingRequest)request {
  __weak __typeof(self) weakSelf = self;
  _service->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kBlingPrototyping, request,
      /*execution_timeout*/ std::nullopt,
      base::BindOnce(
          ^(optimization_guide::OptimizationGuideModelExecutionResult result,
            std::unique_ptr<optimization_guide::ModelQualityLogEntry> entry) {
            [weakSelf onServerModelExecuteResponse:std::move(result)];
          }));
}

- (void)executeOnDeviceQuery:(optimization_guide::proto::StringValue)request {
  if (!_on_device_session) {
    [self.consumer updateQueryResult:@"Session is not ready for querying yet."];
    [self startOnDeviceSession];
    return;
  }
  __weak __typeof(self) weakSelf = self;
  _on_device_session->ExecuteModel(
      request,
      base::RepeatingCallback(base::BindRepeating(
          ^(optimization_guide::OptimizationGuideModelStreamingExecutionResult
                result) {
            [weakSelf onDeviceModelExecuteResponse:std::move(result)];
          })));
}

#pragma mark - Private

// Handles the response from a server-hosted query execution.
- (void)onServerModelExecuteResponse:
    (optimization_guide::OptimizationGuideModelExecutionResult)result {
  std::string response = "";

  if (result.response.has_value()) {
    auto parsed = optimization_guide::ParsedAnyMetadata<
        optimization_guide::proto::BlingPrototypingResponse>(
        result.response.value());
    if (!parsed->output().empty()) {
      response = parsed->output();
    } else {
      response = "Empty server response.";
    }
  } else {
    response =
        base::StringPrintf("Server model execution error: %d",
                           static_cast<int>(result.response.error().error()));
  }

  [self.consumer updateQueryResult:base::SysUTF8ToNSString(response)];
}

// Handles the response from an on-device query execution.
- (void)onDeviceModelExecuteResponse:
    (optimization_guide::OptimizationGuideModelStreamingExecutionResult)result {
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
      _on_device_session.reset();
    }
  } else {
    response =
        base::StringPrintf("On-device model execution error: %d",
                           static_cast<int>(result.response.error().error()));
  }

  [self.consumer updateQueryResult:base::SysUTF8ToNSString(response)];
}

// Attempts to create an on-device session. If the feature's configuration
// hasn't been downloaded yet, this will trigger that download and fail to start
// the session. Once the configuration download is complete, the session will be
// able to be started successfully.
- (void)startOnDeviceSession {
  optimization_guide::SessionConfigParams configParams =
      optimization_guide::SessionConfigParams{
          .execution_mode = optimization_guide::SessionConfigParams::
              ExecutionMode::kOnDeviceOnly,
          .logging_mode = optimization_guide::SessionConfigParams::LoggingMode::
              kAlwaysDisable,
      };
  _on_device_session = _service->StartSession(
      optimization_guide::ModelBasedCapabilityKey::kPromptApi, configParams);
}
#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

@end
