// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/ui/policy/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>

#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "components/cast/message_port/fuchsia/create_web_message.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast_streaming/browser/test/cast_streaming_test_sender.h"
#include "fuchsia_web/cast_streaming/cast_streaming.h"
#include "fuchsia_web/common/init_logging.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/shell/remote_debugging_port.h"
#include "fuchsia_web/webengine/switches.h"
#include "fuchsia_web/webinstance_host/web_instance_host.h"
#include "media/base/media_util.h"

namespace {

void PrintUsage() {
  std::cerr << "Usage: "
            << base::CommandLine::ForCurrentProcess()->GetProgram().BaseName()
            << " [--" << kRemoteDebuggingPortSwitch
            << "=<port>] [-- [--{extra_flag1}] [--{extra_flag2}]]\n"
            << "Setting " << kRemoteDebuggingPortSwitch << "=0"
            << " will automatically choose an available port.\n"
            << "Extra flags will be passed to WebEngine to be processed.\n";
}

media::VideoDecoderConfig GetDefaultVideoConfig() {
  constexpr gfx::Size kVideoSize = {640, 240};
  constexpr gfx::Rect kVideoRect(kVideoSize);

  return media::VideoDecoderConfig(
      media::VideoCodec::kVP8, media::VideoCodecProfile::VP8PROFILE_MIN,
      media::VideoDecoderConfig::AlphaMode::kIsOpaque, media::VideoColorSpace(),
      media::VideoTransformation(), kVideoSize, kVideoRect, kVideoSize,
      media::EmptyExtraData(), media::EncryptionScheme::kUnencrypted);
}

// Set up content directory and context params.
fuchsia::web::CreateContextParams GetCreateContextParams(
    absl::optional<uint16_t> remote_debugging_port) {
  // Configure the fuchsia-dir://cast-streaming/ directory.
  fuchsia::web::CreateContextParams create_context_params;
  ApplyCastStreamingContextParams(&create_context_params);

  // Share this process' service directory with the WebEngine Context
  create_context_params.set_service_directory(
      base::OpenDirectoryHandle(base::FilePath(base::kServiceDirectoryPath)));

  // Enable other WebEngine features.
  fuchsia::web::ContextFeatureFlags features =
      fuchsia::web::ContextFeatureFlags::AUDIO |
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER |
      fuchsia::web::ContextFeatureFlags::NETWORK |
      fuchsia::web::ContextFeatureFlags::VULKAN;
  create_context_params.set_features(features);

  create_context_params.set_remote_debugging_port(*remote_debugging_port);

  return create_context_params;
}

// Set autoplay, enable all logging, and present fullscreen view of `frame`.
void ConfigureFrame(fuchsia::web::Frame* frame) {
  fuchsia::web::ContentAreaSettings settings;
  settings.set_autoplay_policy(fuchsia::web::AutoplayPolicy::ALLOW);
  frame->SetContentAreaSettings(std::move(settings));

  frame->SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel::DEBUG);

  auto view_tokens = scenic::ViewTokenPair::New();
  frame->CreateView(std::move(view_tokens.view_token));
  auto presenter = base::ComponentContextForProcess()
                       ->svc()
                       ->Connect<fuchsia::ui::policy::Presenter>();
  presenter->PresentOrReplaceView(std::move(view_tokens.view_holder_token),
                                  nullptr);
}

}  // namespace

int main(int argc, char** argv) {
  base::SingleThreadTaskExecutor executor(base::MessagePumpType::IO);

  // Parse the command line arguments and set up logging.
  CHECK(base::CommandLine::Init(argc, argv));
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  CHECK(InitLoggingFromCommandLineDefaultingToStderrForTest(command_line));

  absl::optional<uint16_t> remote_debugging_port =
      GetRemoteDebuggingPort(*command_line);
  if (!remote_debugging_port) {
    PrintUsage();
    return 1;
  }

  // Instantiate Web Instance Host.
  WebInstanceHost web_instance_host;
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
  frame_params.set_enable_remote_debugging(true);

  fuchsia::web::FramePtr frame;
  context->CreateFrameWithParams(std::move(frame_params), frame.NewRequest());
  frame.set_error_handler(
      [quit_run_loop = run_loop.QuitClosure()](zx_status_t status) {
        ZX_LOG(ERROR, status) << "Frame connection lost:";
        quit_run_loop.Run();
      });

  ConfigureFrame(frame.get());

  // Register the MessagePort for the Cast Streaming Receiver.
  std::unique_ptr<cast_api_bindings::MessagePort> sender_message_port;
  std::unique_ptr<cast_api_bindings::MessagePort> receiver_message_port;
  cast_api_bindings::CreatePlatformMessagePortPair(&sender_message_port,
                                                   &receiver_message_port);

  constexpr char kCastStreamingMessagePortOrigin[] = "cast-streaming:receiver";
  frame->PostMessage(kCastStreamingMessagePortOrigin,
                     CreateWebMessage("", std::move(receiver_message_port)),
                     [quit_run_loop = run_loop.QuitClosure()](
                         fuchsia::web::Frame_PostMessage_Result result) {
                       if (result.is_err()) {
                         LOG(ERROR) << "PostMessage failed.";
                         quit_run_loop.Run();
                       }
                     });

  // Send `sender_message_port` to a Sender and start it.
  cast_streaming::CastStreamingTestSender sender;
  sender.Start(std::move(sender_message_port), net::IPAddress::IPv6Localhost(),
               absl::nullopt, GetDefaultVideoConfig());

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
  context->GetRemoteDebuggingPort(
      [quit_run_loop = run_loop.QuitClosure()](
          fuchsia::web::Context_GetRemoteDebuggingPort_Result result) {
        if (result.is_err()) {
          LOG(ERROR) << "Remote debugging service was not opened.";
          quit_run_loop.Run();
          return;
        }
        LOG(INFO) << "Remote debugging port: " << result.response().port;
      });

  if (!sender.RunUntilActive()) {
    LOG(ERROR) << "RunUntilActive failed.";
    return 1;
  }

  run_loop.Run();

  return 0;
}
