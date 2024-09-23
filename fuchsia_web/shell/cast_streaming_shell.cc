// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/element/cpp/fidl.h>
#include <fuchsia/io/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>

#include <optional>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/fuchsia/process_context.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/cast/message_port/fuchsia/create_web_message.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast_streaming/test/cast_streaming_test_sender.h"
#include "components/fuchsia_component_support/annotations_manager.h"
#include "fuchsia_web/cast_streaming/cast_streaming.h"
#include "fuchsia_web/common/init_logging.h"
#include "fuchsia_web/common/test/fit_adapter.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/shell/present_frame.h"
#include "fuchsia_web/shell/remote_debugging_port.h"
#include "fuchsia_web/shell/shell_relauncher.h"
#include "fuchsia_web/webengine/switches.h"
#include "fuchsia_web/webinstance_host/web_instance_host.h"
#include "media/base/media_util.h"
#include "media/gpu/test/video_test_helpers.h"

namespace {

// Identifier for JavaScript to be injected, only relevant if injecting multiple
// JavaScripts.
constexpr int kAddBeforeLoadJavaScriptID = 0;

media::VideoDecoderConfig GetDefaultVideoConfig() {
  constexpr gfx::Size kVideoSize = {1280, 720};
  constexpr gfx::Rect kVideoRect(kVideoSize);

  return media::VideoDecoderConfig(
      media::VideoCodec::kVP8, media::VideoCodecProfile::VP8PROFILE_MIN,
      media::VideoDecoderConfig::AlphaMode::kIsOpaque, media::VideoColorSpace(),
      media::VideoTransformation(), kVideoSize, kVideoRect, kVideoSize,
      media::EmptyExtraData(), media::EncryptionScheme::kUnencrypted);
}

// Set up content directory and context params.
fuchsia::web::CreateContextParams GetCreateContextParams(
    std::optional<uint16_t> remote_debugging_port) {
  // Configure the fuchsia-dir://cast-streaming/ directory.
  fuchsia::web::CreateContextParams create_context_params;
  ApplyCastStreamingContextParams(&create_context_params);

  // Enable other WebEngine features.
  fuchsia::web::ContextFeatureFlags features =
      fuchsia::web::ContextFeatureFlags::AUDIO |
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER |
      fuchsia::web::ContextFeatureFlags::NETWORK |
      fuchsia::web::ContextFeatureFlags::VULKAN;
  create_context_params.set_features(features);

  if (remote_debugging_port) {
    create_context_params.set_remote_debugging_port(*remote_debugging_port);
  }

  return create_context_params;
}

// Set autoplay, enable all logging, and present fullscreen view of `frame`.
std::optional<fuchsia::element::GraphicalPresenterPtr> ConfigureFrame(
    fuchsia::web::Frame* frame,
    fidl::InterfaceHandle<fuchsia::element::AnnotationController>
        annotation_controller) {
  fuchsia::web::ContentAreaSettings settings;
  settings.set_autoplay_policy(fuchsia::web::AutoplayPolicy::ALLOW);
  frame->SetContentAreaSettings(std::move(settings));
  frame->SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel::DEBUG);
  return PresentFrame(frame, std::move(annotation_controller));
}

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  CHECK(InitLoggingFromCommandLine(*command_line));

  base::SingleThreadTaskExecutor executor(base::MessagePumpType::IO);

  if (auto optional_exit_code = RelaunchForWebInstanceHostIfParent(
          "#meta/cast_streaming_shell_for_web_instance_host.cm", *command_line);
      optional_exit_code.has_value()) {
    return optional_exit_code.value();
  }

  std::optional<uint16_t> remote_debugging_port =
      GetRemoteDebuggingPort(*command_line);

  // Instantiate Web Instance Host.
  WebInstanceHostWithServicesFromThisComponent web_instance_host(
      *base::ComponentContextForProcess()->outgoing(),
      /*is_web_instance_component_in_same_package=*/false);
  fidl::InterfaceRequest<fuchsia::io::Directory> services_request;
  auto services = sys::ServiceDirectory::CreateWithRequest(&services_request);
  base::CommandLine child_command_line =
      base::CommandLine(command_line->GetArgs());
  child_command_line.AppendSwitch(switches::kEnableCastStreamingReceiver);
  zx_status_t result = web_instance_host.CreateInstanceForContextWithCopiedArgs(
      GetCreateContextParams(remote_debugging_port),
      std::move(services_request), child_command_line);
  if (result != ZX_OK) {
    ZX_LOG(ERROR, result) << "CreateInstanceForContextWithCopiedArgs failed";
    return 2;
  }

  base::ComponentContextForProcess()->outgoing()->ServeFromStartupInfo();

  // Create the browser `context`.
  fuchsia::web::ContextPtr context;
  services->Connect(context.NewRequest());

  base::RunLoop run_loop;

  context.set_error_handler(
      [quit_run_loop = run_loop.QuitClosure()](zx_status_t status) {
        ZX_LOG(ERROR, status) << "Context connection lost:";
        quit_run_loop.Run();
      });

  // Create the browser `frame`.
  fuchsia::web::CreateFrameParams frame_params;
  if (remote_debugging_port) {
    frame_params.set_enable_remote_debugging(true);
  }

  fuchsia::web::FramePtr frame;
  context->CreateFrameWithParams(std::move(frame_params), frame.NewRequest());
  frame.set_error_handler(
      [quit_run_loop = run_loop.QuitClosure()](zx_status_t status) {
        ZX_LOG(ERROR, status) << "Frame connection lost:";
        quit_run_loop.Run();
      });

  // The underlying PresentView call expects an AnnotationController and will
  // return PresentViewError.INVALID_ARGS without one. The AnnotationController
  // should serve WatchAnnotations, but it doesn't need to do anything.
  // TODO(b/264899156): Remove this when AnnotationController becomes
  // optional.
  auto annotations_manager =
      std::make_unique<fuchsia_component_support::AnnotationsManager>();
  fuchsia::element::AnnotationControllerPtr annotation_controller;
  annotations_manager->Connect(annotation_controller.NewRequest());
  auto presenter =
      ConfigureFrame(frame.get(), std::move(annotation_controller));

  // Register the MessagePort for the Cast Streaming Receiver.
  std::unique_ptr<cast_api_bindings::MessagePort> sender_message_port;
  std::unique_ptr<cast_api_bindings::MessagePort> receiver_message_port;
  cast_api_bindings::CreatePlatformMessagePortPair(&sender_message_port,
                                                   &receiver_message_port);

  constexpr char kCastStreamingMessagePortOrigin[] = "cast-streaming:receiver";
  base::test::TestFuture<fuchsia::web::Frame_PostMessage_Result>
      post_message_result;
  frame->PostMessage(kCastStreamingMessagePortOrigin,
                     CreateWebMessage("", std::move(receiver_message_port)),
                     CallbackToFitFunction(post_message_result.GetCallback()));
  if (!post_message_result.Wait()) {
    LOG(ERROR) << "PostMessage timed out.";
    return 1;
  }
  if (post_message_result.Get().is_err()) {
    LOG(ERROR) << "PostMessage failed.";
    return 1;
  }

  // Inject JavaScript test harness into receiver.html.
  base::test::TestFuture<fuchsia::web::Frame_AddBeforeLoadJavaScript_Result>
      add_before_load_javascript_result;
  base::FilePath pkg_path;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &pkg_path));
  base::FilePath test_harness_js_path(pkg_path.AppendASCII(
      "fuchsia_web/shell/cast_streaming_shell_data/injector.js"));
  std::string test_harness_string;
  CHECK(base::ReadFileToString(test_harness_js_path, &test_harness_string));
  frame->AddBeforeLoadJavaScript(
      kAddBeforeLoadJavaScriptID, {"*"},
      base::MemBufferFromString(std::move(test_harness_string),
                                "test-harness-js"),
      CallbackToFitFunction(add_before_load_javascript_result.GetCallback()));
  if (!add_before_load_javascript_result.Wait()) {
    LOG(ERROR) << "AddBeforeLoadJavaScript timed out.";
    return 1;
  }
  if (add_before_load_javascript_result.Get().is_err()) {
    LOG(ERROR) << "AddBeforeLoadJavaScript failed.";
    return 1;
  }

  // Send `sender_message_port` to a Sender and start it.
  cast_streaming::CastStreamingTestSender sender;
  sender.Start(std::move(sender_message_port), net::IPAddress::IPv6Localhost(),
               std::nullopt, GetDefaultVideoConfig());

  // Navigate `frame` to `receiver.html`.
  fuchsia::web::LoadUrlParams load_params;
  load_params.set_type(fuchsia::web::LoadUrlReason::TYPED);
  load_params.set_was_user_activated(true);
  fuchsia::web::NavigationControllerPtr nav_controller;
  frame->GetNavigationController(nav_controller.NewRequest());
  if (!LoadUrlAndExpectResponse(nav_controller, std::move(load_params),
                                kCastStreamingWebUrl)) {
    LOG(ERROR) << "LoadUrl failed.";
    return 1;
  }

  // Log the debugging port, if debugging is requested.
  base::test::TestFuture<fuchsia::web::Context_GetRemoteDebuggingPort_Result>
      get_remote_debugging_port_result;
  context->GetRemoteDebuggingPort(
      CallbackToFitFunction(get_remote_debugging_port_result.GetCallback()));
  if (!get_remote_debugging_port_result.Wait()) {
    LOG(ERROR) << "Remote debugging service timed out.";
    return 1;
  }
  if (get_remote_debugging_port_result.Get().is_err()) {
    LOG(ERROR) << "Remote debugging service was not opened.";
    return 1;
  }
  LOG(INFO) << "Remote debugging port: "
            << get_remote_debugging_port_result.Get().response().port;

  if (!sender.RunUntilActive()) {
    LOG(ERROR) << "RunUntilActive failed.";
    return 1;
  }

  // Load video.
  base::FilePath video_file(
      pkg_path.AppendASCII("media/test/data/bear-1280x720.ivf"));
  std::optional<std::vector<uint8_t>> video_stream =
      base::ReadFileToBytes(video_file);
  CHECK(video_stream.has_value());
  auto video_helper = media::test::EncodedDataHelper::Create(
      video_stream.value(), media::VideoCodec::kVP8);

  // Send first key frame.
  scoped_refptr<media::DecoderBuffer> video_decoder_buffer =
      video_helper->GetNextBuffer();
  video_decoder_buffer->set_timestamp(base::TimeDelta());
  video_decoder_buffer->set_is_key_frame(true);
  sender.SendVideoBuffer(video_decoder_buffer);

  run_loop.Run();

  return 0;
}
