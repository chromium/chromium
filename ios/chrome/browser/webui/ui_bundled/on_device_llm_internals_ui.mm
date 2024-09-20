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
#import "components/optimization_guide/internal/third_party/odml/src/odml/infra/genai/inference/c/llm_inference_engine.h"  // nogncheck
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
  std::string model_name = BUILDFLAG(IOS_ON_DEVICE_LLM_NAME);
  if (model_name.empty()) {
    model_name = "(No model loaded)";
  }

  base::ValueView js_args[] = {model_name};
  web_ui()->CallJavascriptFunction("updateModelInformation", js_args);
}

void OnDeviceLlmInternalsHandler::InitAndGenerateResponse(
    const base::Value::List& args) {
  CHECK(args.size() == 1);

// iOS is bring-your-own-model. To enable the on-device code:
// Run `gn args out/target` or add the following to `~/.setup-gn`
// ios_on_device_llm_path = /path/to/model.bin
#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  VLOG(1) << "Init LLM and generate response...";
  VLOG(1) << "query: " << args[0];

  std::string cache_dir = base::SysNSStringToUTF8(
      [[[NSFileManager defaultManager] temporaryDirectory] path]);
  VLOG(1) << "cache_dir: " << cache_dir;

  NSString* model_file_name =
      base::SysUTF8ToNSString(BUILDFLAG(IOS_ON_DEVICE_LLM_NAME));
  NSString* model_file_extension =
      base::SysUTF8ToNSString(BUILDFLAG(IOS_ON_DEVICE_LLM_EXTENSION));
  std::string model_file_path = base::SysNSStringToUTF8([[NSBundle mainBundle]
      pathForResource:model_file_name
               ofType:model_file_extension]);
  VLOG(1) << "model_file_path: " << model_file_path;

  const LlmModelSettings model_settings = {
      .model_path = model_file_path.c_str(),
      .cache_dir = cache_dir.c_str(),
      .max_num_tokens = 512,
      .num_decode_steps_per_sync = 3,
      .sequence_batch_size = 0,
  };

  const LlmSessionConfig session_config = {
      .topk = 40,
      .topp = 1.0f,
      .temperature = 0.8f,
      .random_seed = 0,
  };

  // Create the engine.
  char* error = nullptr;
  LlmInferenceEngine_Engine* llm_engine = nullptr;
  int error_code =
      LlmInferenceEngine_CreateEngine(&model_settings, &llm_engine, &error);
  VLOG(1) << "LlmInferenceEngine_CreateEngine error code: " << error_code;
  VLOG_IF(1, error_code != 0)
      << "LlmInferenceEngine_CreateEngine error message: " << error;

  // Create the session.
  LlmInferenceEngine_Session* llm_session = nullptr;
  error_code = LlmInferenceEngine_CreateSession(llm_engine, &session_config,
                                                &llm_session, &error);
  VLOG(1) << "LlmInferenceEngine_CreateSession error code: " << error_code;
  VLOG_IF(1, error_code != 0)
      << "LlmInferenceEngine_CreateSession error message: " << error;

  // Run the inference.
  // TODO(crbug.com/356608952): Not on the main thread.
  error_code = LlmInferenceEngine_Session_AddQueryChunk(
      llm_session, args[0].GetString().c_str(), &error);
  VLOG(1) << "LlmInferenceEngine_Session_AddQueryChunk error code: "
          << error_code;
  VLOG_IF(1, error_code != 0)
      << "LlmInferenceEngine_Session_AddQueryChunk error message: " << error;

  LlmResponseContext llm_response_context =
      LlmInferenceEngine_Session_PredictSync(llm_session);

  std::string response = std::string(llm_response_context.response_array[0]);
  VLOG(1) << "LLM internals: response: " << response;

  // Delete the inference objects.
  // TODO(crbug.com/356608952): Reuse these across runs.
  LlmInferenceEngine_CloseResponseContext(&llm_response_context);
  LlmInferenceEngine_Session_Delete(llm_session);
  llm_session = nullptr;
  LlmInferenceEngine_Engine_Delete(llm_engine);
  llm_engine = nullptr;
#else
  std::string response = "No model loaded.";
#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

  base::ValueView js_args[] = {response};
  web_ui()->CallJavascriptFunction("showResult", js_args);
}

OnDeviceLlmInternalsUI::OnDeviceLlmInternalsUI(web::WebUIIOS* web_ui,
                                               const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web_ui->AddMessageHandler(std::make_unique<OnDeviceLlmInternalsHandler>());

  web::WebUIIOSDataSource::Add(ProfileIOS::FromWebUIIOS(web_ui),
                               CreateOnDeviceLlmInternalsUIHTMLSource());
}

OnDeviceLlmInternalsUI::~OnDeviceLlmInternalsUI() {}
