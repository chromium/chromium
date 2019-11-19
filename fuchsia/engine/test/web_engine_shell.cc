// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/ui/policy/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>

#include "base/base_paths_fuchsia.h"
#include "base/command_line.h"
#include "base/fuchsia/default_context.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "fuchsia/base/init_logging.h"
#include "url/gurl.h"

constexpr char kRemoteDebuggingPortSwitch[] = "remote-debugging-port";
constexpr char kEnableLoggingSwitch[] = "enable-logging";

void PrintUsage() {
  LOG(INFO) << "Usage: "
            << base::CommandLine::ForCurrentProcess()->GetProgram().BaseName()
            << " [--" << kRemoteDebuggingPortSwitch << "] URL";
}

int main(int argc, char** argv) {
  base::SingleThreadTaskExecutor executor(base::MessagePumpType::IO);

  // Parse the command line arguments and set up logging.
  CHECK(base::CommandLine::Init(argc, argv));
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchNative(kEnableLoggingSwitch, "stderr");

  CHECK(cr_fuchsia::InitLoggingFromCommandLine(*command_line));
  base::CommandLine::StringVector args =
      base::CommandLine::ForCurrentProcess()->GetArgs();
  if (args.empty()) {
    LOG(ERROR) << "No URL provided.";
    PrintUsage();
    return 1;
  }
  if (args.size() > 1) {
    LOG(ERROR) << "Too many arguments provided.";
    PrintUsage();
    return 1;
  }
  GURL url(args.front());
  if (!url.is_valid()) {
    LOG(ERROR) << "URL is not valid: " << url.spec();
    PrintUsage();
    return 1;
  }
  base::Optional<uint16_t> remote_debugging_port;
  if (command_line->HasSwitch(kRemoteDebuggingPortSwitch)) {
    std::string port_str =
        command_line->GetSwitchValueNative(kRemoteDebuggingPortSwitch);
    int port_parsed;
    if (!base::StringToInt(port_str, &port_parsed) || port_parsed <= 0 ||
        port_parsed > 65535) {
      LOG(ERROR) << "Invalid value for --remote-debugging-port (must be in the "
                    "range 1-65535).";
      PrintUsage();
      return 1;
    }
    remote_debugging_port = base::checked_cast<uint16_t>(port_parsed);
  }

  auto web_context_provider = base::fuchsia::ComponentContextForCurrentProcess()
                                  ->svc()
                                  ->Connect<fuchsia::web::ContextProvider>();

  // Set up the content directory fuchsia-pkg://shell-data/, which will host
  // the files stored under //fuchsia/engine/test/shell_data.
  fuchsia::web::CreateContextParams create_context_params;
  fuchsia::web::ContentDirectoryProvider content_directory;
  base::FilePath pkg_path;
  base::PathService::Get(base::DIR_ASSETS, &pkg_path);
  content_directory.set_directory(base::fuchsia::OpenDirectory(
      pkg_path.AppendASCII("fuchsia/engine/test/shell_data")));
  content_directory.set_name("shell-data");
  std::vector<fuchsia::web::ContentDirectoryProvider> content_directories;
  content_directories.emplace_back(std::move(content_directory));
  create_context_params.set_content_directories(
      {std::move(content_directories)});

  // WebEngine Contexts can only make use of the services provided by the
  // embedder application. By passing a handle to this process' service
  // directory to the ContextProvider, we are allowing the Context access to the
  // same set of services available to this application.
  create_context_params.set_service_directory(base::fuchsia::OpenDirectory(
      base::FilePath(base::fuchsia::kServiceDirectoryPath)));

  // Enable other WebEngine features.
  create_context_params.set_features(
      fuchsia::web::ContextFeatureFlags::AUDIO |
      fuchsia::web::ContextFeatureFlags::VULKAN |
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER |
      fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM);
  if (remote_debugging_port)
    create_context_params.set_remote_debugging_port(*remote_debugging_port);

  base::RunLoop run_loop;

  // Create the browser |context|.
  fuchsia::web::ContextPtr context;
  web_context_provider->Create(std::move(create_context_params),
                               context.NewRequest());
  context.set_error_handler(
      [quit_run_loop = run_loop.QuitClosure()](zx_status_t status) {
        ZX_LOG(ERROR, status) << "Context connection lost:";
        quit_run_loop.Run();
      });

  // Create the browser |frame| which will contain the webpage.
  fuchsia::web::CreateFrameParams frame_params;
  if (remote_debugging_port)
    frame_params.set_enable_remote_debugging(true);

  fuchsia::web::FramePtr frame;
  context->CreateFrameWithParams(std::move(frame_params), frame.NewRequest());
  frame.set_error_handler(
      [quit_run_loop = run_loop.QuitClosure()](zx_status_t status) {
        ZX_LOG(ERROR, status) << "Frame connection lost:";
        quit_run_loop.Run();
      });

  // Navigate |frame| to |url|.
  fuchsia::web::LoadUrlParams load_params;
  load_params.set_type(fuchsia::web::LoadUrlReason::TYPED);
  load_params.set_was_user_activated(true);
  fuchsia::web::NavigationControllerPtr nav_controller;
  frame->GetNavigationController(nav_controller.NewRequest());
  nav_controller->LoadUrl(
      url.spec(), std::move(load_params),
      [quit_run_loop = run_loop.QuitClosure()](
          fuchsia::web::NavigationController_LoadUrl_Result result) {
        if (result.is_err()) {
          LOG(ERROR) << "LoadUrl failed.";
          quit_run_loop.Run();
        }
      });

  // Present a fullscreen view of |frame|.
  fuchsia::ui::views::ViewToken view_token;
  fuchsia::ui::views::ViewHolderToken view_holder_token;
  std::tie(view_token, view_holder_token) = scenic::NewViewTokenPair();
  frame->CreateView(std::move(view_token));
  auto presenter = base::fuchsia::ComponentContextForCurrentProcess()
                       ->svc()
                       ->Connect<::fuchsia::ui::policy::Presenter>();
  presenter->PresentView(std::move(view_holder_token), nullptr);

  LOG(INFO) << "Launched browser at URL " << url.spec();

  // Run until the process is killed with CTRL-C or the connections to Web
  // Engine interfaces are dropped.
  run_loop.Run();

  return 0;
}
