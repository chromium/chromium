// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/on_device_llm_internals_ui.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/optimization_guide/optimization_guide_buildflags.h"
#import "ios/chrome/browser/optimization_guide/resources/on_device_llm_buildflags.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/grit/ios_resources.h"
#import "ios/web/public/webui/url_data_source_ios.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ios/web/public/webui/web_ui_ios_message_handler.h"

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
#import "base/memory/weak_ptr.h"
#import "base/strings/stringprintf.h"
#import "components/optimization_guide/core/model_execution/feature_keys.h"  // nogncheck
#import "components/optimization_guide/core/model_execution/on_device_model_component.h"  // nogncheck
#import "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"  // nogncheck
#import "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"  // nogncheck
#import "components/optimization_guide/core/optimization_guide_constants.h"  // nogncheck
#import "components/optimization_guide/core/optimization_guide_features.h"  // nogncheck
#import "components/optimization_guide/core/optimization_guide_switches.h"  // nogncheck
#import "components/optimization_guide/core/optimization_guide_util.h"  // nogncheck
#import "components/optimization_guide/machine_learning_tflite_buildflags.h"  // nogncheck
#import "components/optimization_guide/proto/model_execution.pb.h"  // nogncheck
#import "components/optimization_guide/proto/model_validation.pb.h"  // nogncheck
#import "components/optimization_guide/proto/string_value.pb.h"  // nogncheck
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"  // nogncheck
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"  // nogncheck
#endif

namespace {

web::WebUIIOSDataSource* CreateOnDeviceLlmInternalsUIHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIOnDeviceLlmInternalsHost);

  source->SetDefaultResource(IDR_IOS_ON_DEVICE_LLM_INTERNALS_HTML);
  return source;
}

class OnDeviceLlmInternalsHandler : public web::WebUIIOSMessageHandler {
 public:
  OnDeviceLlmInternalsHandler();

  OnDeviceLlmInternalsHandler(const OnDeviceLlmInternalsHandler&) = delete;
  OnDeviceLlmInternalsHandler& operator=(const OnDeviceLlmInternalsHandler&) =
      delete;

  ~OnDeviceLlmInternalsHandler() override;

  // WebUIIOSMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleRequestModelInformation(const base::Value::List& args);
  void InitAndGenerateResponse(const base::Value::List& args);

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  void OnServerModelExecuteResponse(
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);
  void OnDeviceModelExecuteResponse(
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  // Retains the on-device session in memory.
  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      on_device_session_;
#endif

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<OnDeviceLlmInternalsHandler> weak_ptr_factory_{this};
};

}  // namespace

OnDeviceLlmInternalsHandler::OnDeviceLlmInternalsHandler() {}

OnDeviceLlmInternalsHandler::~OnDeviceLlmInternalsHandler() {}

void OnDeviceLlmInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestModelInformation",
      base::BindRepeating(
          &OnDeviceLlmInternalsHandler::HandleRequestModelInformation,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initAndGenerateResponse",
      base::BindRepeating(&OnDeviceLlmInternalsHandler::InitAndGenerateResponse,
                          base::Unretained(this)));
}

void OnDeviceLlmInternalsHandler::HandleRequestModelInformation(
    const base::Value::List& args) {
  // TODO(crbug.com/370768381): Load model name.
  std::string model_name = "";
  if (model_name.empty()) {
    model_name = "(Model name unavailable)";
  }

  base::ValueView js_args[] = {model_name};
  web_ui()->CallJavascriptFunction("updateModelInformation", js_args);
}

void OnDeviceLlmInternalsHandler::InitAndGenerateResponse(
    const base::Value::List& args) {
  CHECK(args.size() == 1);
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

  // iOS is bring-your-own-model. To enable the on-device code:
  // Run `gn args out/target` or add the following to `~/.setup-gn`
  // ios_on_device_llm_files = [/path/to/weights.bin, /path/to/manifest.json,
  // /path/to/config.pb]
  if (!args[0].is_string()) {
    LOG(ERROR) << "Invalid input";
    return;
  }

  std::string input = args[0].GetString();
  VLOG(1) << "Init LLM and generate response...";
  VLOG(1) << "query: " << input;

  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui());
  OptimizationGuideService* service =
      OptimizationGuideServiceFactory::GetForProfile(profile);

#if 0   // Server inference
  optimization_guide::proto::StringValue request;
  request.set_value(input);
  VLOG(1) << "Executing server query";
  service->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kTest, request,
      /*execution_timeout=*/std::nullopt,
      base::BindOnce(&OnDeviceLlmInternalsHandler::OnServerModelExecuteResponse,
                     weak_ptr_factory_.GetWeakPtr()));
#endif  // Server inference

#if 1  // On-device inference
  if (!on_device_session_) {
    VLOG(1) << "Starting on-device session";
    optimization_guide::SessionConfigParams config_params =
        optimization_guide::SessionConfigParams{
            .execution_mode = optimization_guide::SessionConfigParams::
                ExecutionMode::kOnDeviceOnly};
    on_device_session_ = service->StartSession(
        optimization_guide::ModelBasedCapabilityKey::kPromptApi, config_params);

    if (!on_device_session_) {
      LOG(ERROR) << "On-device session failed to start";
      std::string response = "On-device session failed to start.";
      base::ValueView js_args[] = {response};
      web_ui()->CallJavascriptFunction("showResult", js_args);
      return;
    }
  }

  optimization_guide::proto::StringValue request;
  request.set_value(input);
  VLOG(1) << "Executing on-device query";
  on_device_session_->ExecuteModel(
      request, base::RepeatingCallback(base::BindRepeating(
                   &OnDeviceLlmInternalsHandler::OnDeviceModelExecuteResponse,
                   weak_ptr_factory_.GetWeakPtr())));

#endif  // On-device inference
#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
}

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
void OnDeviceLlmInternalsHandler::OnServerModelExecuteResponse(
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  std::string response = "";

  if (result.response.has_value()) {
    auto parsed = optimization_guide::ParsedAnyMetadata<
        optimization_guide::proto::StringValue>(result.response.value());
    if (parsed->has_value()) {
      response = parsed->value();
    } else {
      response = "Failed to parse server response as a string";
    }
  } else {
    response =
        base::StringPrintf("Server model execution error: %d",
                           static_cast<int>(result.response.error().error()));
  }

  VLOG(1) << "Server query response: " << response;
  base::ValueView js_args[] = {response};
  web_ui()->CallJavascriptFunction("showResult", js_args);
}

void OnDeviceLlmInternalsHandler::OnDeviceModelExecuteResponse(
    optimization_guide::OptimizationGuideModelStreamingExecutionResult result) {
  std::string response = "";

  if (!result.response.has_value() || result.response->is_complete) {
    VLOG(1) << "On-device execution complete";
    base::ValueView js_args[] = {"Unidentified model loaded"};
    web_ui()->CallJavascriptFunction("updateModelInformation", js_args);
    return;
  }

  auto parsed = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::StringValue>(result.response->response);
  if (parsed->has_value()) {
    LOG(ERROR) << parsed->value();
    response = parsed->value();
  } else {
    response = "Failed to parse device response as a string";
  }

  VLOG(1) << "On-device query response: " << response;
  base::ValueView js_args[] = {response};
  web_ui()->CallJavascriptFunction("showResult", js_args);
}
#endif

OnDeviceLlmInternalsUI::OnDeviceLlmInternalsUI(web::WebUIIOS* web_ui,
                                               const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web_ui->AddMessageHandler(std::make_unique<OnDeviceLlmInternalsHandler>());

  web::WebUIIOSDataSource::Add(ProfileIOS::FromWebUIIOS(web_ui),
                               CreateOnDeviceLlmInternalsUIHTMLSource());
}

OnDeviceLlmInternalsUI::~OnDeviceLlmInternalsUI() {}
