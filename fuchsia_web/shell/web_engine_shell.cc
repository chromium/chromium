// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/element/cpp/fidl.h>
#include <fuchsia/io/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fidl/cpp/interface_ptr.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>

#include <iostream>
#include <optional>
#include <utility>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_executor.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/fuchsia_component_support/annotations_manager.h"
#include "fuchsia_web/common/init_logging.h"
#include "fuchsia_web/shell/present_frame.h"
#include "fuchsia_web/shell/remote_debugging_port.h"
#include "fuchsia_web/shell/shell_relauncher.h"
#include "fuchsia_web/webinstance_host/web_instance_host.h"
#include "fuchsia_web/webinstance_host/web_instance_host_constants.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "url/gurl.h"

namespace {

constexpr char kHeadlessSwitch[] = "headless";
constexpr char kEnableProtectedMediaIdentifier[] =
    "enable-protected-media-identifier";
// TODO(crbug.com/40896202): This flag will be removed. Keep for now to prevent
// users from failing.
constexpr char kUseWebInstance[] = "use-web-instance";
constexpr char kUseContextProvider[] = "use-context-provider";
constexpr char kEnableWebInstanceTmp[] = "enable-web-instance-tmp";

void PrintUsage() {
  std::cerr << "Usage: "
            << base::CommandLine::ForCurrentProcess()->GetProgram().BaseName()
            << " [--" << kRemoteDebuggingPortSwitch << "] [--"
            << kHeadlessSwitch << "] [--" << switches::kWithWebui
            << "] URL [--] [--{extra_flag1}] "
            << "[--{extra_flag2}]" << std::endl
            << "Setting " << kRemoteDebuggingPortSwitch << " to 0 will "
            << "automatically choose an available port." << std::endl
            << "Setting " << kHeadlessSwitch << " will prevent creation of "
            << "a view." << std::endl
            << "Extra flags will be passed to "
            << "WebEngine to be processed." << std::endl;
}

GURL GetUrlFromArgs(const base::CommandLine::StringVector& args) {
  if (args.empty()) {
    LOG(ERROR) << "No URL provided.";
    return GURL();
  }
  GURL url = GURL(args.front());
  if (!url.is_valid()) {
    LOG(ERROR) << "URL is not valid: " << url.spec();
    return GURL();
  }
  return url;
}

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  CHECK(InitLoggingFromCommandLine(*command_line));

  base::SingleThreadTaskExecutor executor(base::MessagePumpType::IO);

  const bool use_web_instance = command_line->HasSwitch(kUseWebInstance);
  const bool use_context_provider =
      command_line->HasSwitch(kUseContextProvider);

  if (use_web_instance && use_context_provider) {
    LOG(ERROR) << "Cannot use " << kUseWebInstance << " and "
               << kUseContextProvider << " simultaneously.";
    return 1;
  }

  if (use_web_instance) {
    LOG(WARNING) << "Flag " << kUseWebInstance << " is deprecated and has no "
                 << "effect as WebInstance is used by default.";
  }

  if (!use_context_provider) {
    if (auto optional_exit_code = RelaunchForWebInstanceHostIfParent(
            "#meta/web_engine_shell_for_web_instance_host.cm", *command_line);
        optional_exit_code.has_value()) {
      return optional_exit_code.value();
    }
  }

  std::optional<uint16_t> remote_debugging_port =
      GetRemoteDebuggingPort(*command_line);

  const bool is_headless = command_line->HasSwitch(kHeadlessSwitch);
  const bool enable_protected_media_identifier_access =
      command_line->HasSwitch(kEnableProtectedMediaIdentifier);
  const bool enable_web_instance_tmp =
      command_line->HasSwitch(kEnableWebInstanceTmp);
  const bool with_webui = command_line->HasSwitch(switches::kWithWebui);

  base::CommandLine::StringVector additional_args = command_line->GetArgs();
  GURL url(GetUrlFromArgs(additional_args));
  if (!url.is_valid()) {
    PrintUsage();
    return 1;
  }

  // Remove the url since we don't pass it into WebEngine
  additional_args.erase(additional_args.begin());

  if (enable_web_instance_tmp && use_context_provider) {
    LOG(ERROR) << "Cannot use --enable-web-instance-tmp without "
               << "--use-web-instance";
    return 1;
  }

  if (with_webui && use_context_provider) {
    LOG(ERROR) << "Cannot use --with-webui without --use-web-instance";
    return 1;
  }

  if (!additional_args.empty() && use_context_provider) {
    LOG(ERROR) << "Cannot use extra args without --use-web-instance";
    return 1;
  }

  // Set up the content directory fuchsia-pkg://shell-data/, which will host
  // the files stored under //fuchsia_web/shell/data.
  fuchsia::web::CreateContextParams create_context_params;
  fuchsia::web::ContentDirectoryProvider content_directory;
  base::FilePath pkg_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &pkg_path);
  content_directory.set_directory(base::OpenDirectoryHandle(
      pkg_path.AppendASCII("fuchsia_web/shell/data")));
  content_directory.set_name("shell-data");
  std::vector<fuchsia::web::ContentDirectoryProvider> content_directories;
  content_directories.emplace_back(std::move(content_directory));
  create_context_params.set_content_directories(
      {std::move(content_directories)});

  // Enable other WebEngine features.
  fuchsia::web::ContextFeatureFlags features =
      fuchsia::web::ContextFeatureFlags::AUDIO |
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER |
      fuchsia::web::ContextFeatureFlags::KEYBOARD |
      fuchsia::web::ContextFeatureFlags::NETWORK |
      fuchsia::web::ContextFeatureFlags::VIRTUAL_KEYBOARD;
#if BUILDFLAG(ENABLE_WIDEVINE)
  features |= fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM;
#endif
  if (is_headless) {
    features |= fuchsia::web::ContextFeatureFlags::HEADLESS;
  } else {
    features |= fuchsia::web::ContextFeatureFlags::VULKAN;
  }

  create_context_params.set_features(features);
  if (remote_debugging_port) {
    create_context_params.set_remote_debugging_port(*remote_debugging_port);
  }

  // DRM services require cdm_data_directory to be populated, so create a
  // directory under /data and use that as the cdm_data_directory.
  base::FilePath cdm_data_path =
      base::FilePath(base::kPersistedDataDirectoryPath).Append("cdm_data");
  base::File::Error error;
  CHECK(base::CreateDirectoryAndGetError(cdm_data_path, &error)) << error;
  create_context_params.set_cdm_data_directory(
      base::OpenDirectoryHandle(cdm_data_path));
  CHECK(create_context_params.cdm_data_directory());

  base::RunLoop run_loop;

  fuchsia::web::ContextProviderPtr web_context_provider;
  std::unique_ptr<WebInstanceHost> web_instance_host;
  fuchsia::web::ContextPtr context;
  fuchsia::io::DirectoryHandle tmp_directory;

  if (use_context_provider) {
    // Connect to the system instance of the ContextProvider.
    // WebEngine Contexts can only make use of the services provided by the
    // embedder application. By passing a handle to this process' service
    // directory to the ContextProvider, we are allowing the Context access to
    // the same set of services available to this application.
    create_context_params.set_service_directory(
        base::OpenDirectoryHandle(base::FilePath(base::kServiceDirectoryPath)));
    web_context_provider = base::ComponentContextForProcess()
                               ->svc()
                               ->Connect<fuchsia::web::ContextProvider>();
    web_context_provider->Create(std::move(create_context_params),
                                 context.NewRequest());
  } else {
    // Route services dynamically from web_engine_shell's parent down into
    // created web_instances.
    web_instance_host =
        std::make_unique<WebInstanceHostWithServicesFromThisComponent>(
            *base::ComponentContextForProcess()->outgoing(),
            /*is_web_instance_component_in_same_package=*/false);
    if (enable_web_instance_tmp) {
      const zx_status_t status = fdio_open(
          "/tmp",
          static_cast<uint32_t>(fuchsia::io::OpenFlags::RIGHT_READABLE |
                                fuchsia::io::OpenFlags::RIGHT_WRITABLE |
                                fuchsia::io::OpenFlags::DIRECTORY),
          tmp_directory.NewRequest().TakeChannel().release());
      ZX_CHECK(status == ZX_OK, status) << "fdio_open(/tmp)";
      web_instance_host->set_tmp_dir(std::move(tmp_directory));
    }
    fidl::InterfaceRequest<fuchsia::io::Directory> services_request;
    auto services = sys::ServiceDirectory::CreateWithRequest(&services_request);
    zx_status_t result =
        web_instance_host->CreateInstanceForContextWithCopiedArgs(
            std::move(create_context_params), std::move(services_request),
            base::CommandLine(additional_args));
    if (result == ZX_OK) {
      services->Connect(context.NewRequest());
    } else {
      ZX_LOG(ERROR, result) << "CreateInstanceForContextWithCopiedArgs failed";
      return 2;
    }
  }
  context.set_error_handler(
      [quit_run_loop = run_loop.QuitClosure()](zx_status_t status) {
        ZX_LOG(ERROR, status) << "Context connection lost:";
        quit_run_loop.Run();
      });

  // Create the browser |frame| which will contain the webpage.
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

  fuchsia::web::ContentAreaSettings settings;
  settings.set_autoplay_policy(fuchsia::web::AutoplayPolicy::ALLOW);
  frame->SetContentAreaSettings(std::move(settings));

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

  // Since this is for development, enable all logging.
  frame->SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel::DEBUG);

  if (enable_protected_media_identifier_access) {
    fuchsia::web::PermissionDescriptor protected_media_permission;
    protected_media_permission.set_type(
        fuchsia::web::PermissionType::PROTECTED_MEDIA_IDENTIFIER);
    frame->SetPermissionState(std::move(protected_media_permission),
                              url.DeprecatedGetOriginAsURL().spec(),
                              fuchsia::web::PermissionState::GRANTED);
  }

  // The underlying PresentView call expects an AnnotationController and will
  // return PresentViewError.INVALID_ARGS without one. The AnnotationController
  // should serve WatchAnnotations, but it doesn't need to do anything.
  // TODO(b/264899156): Remove this when AnnotationController becomes
  // optional.
  auto annotations_manager =
      std::make_unique<fuchsia_component_support::AnnotationsManager>();
  fuchsia::element::AnnotationControllerPtr annotation_controller;
  annotations_manager->Connect(annotation_controller.NewRequest());

  fuchsia::element::GraphicalPresenterPtr presenter;
  if (is_headless) {
    frame->EnableHeadlessRendering();
  } else {
    presenter = PresentFrame(frame.get(), std::move(annotation_controller));
  }

  LOG(INFO) << "Launched browser at URL " << url.spec();

  base::ComponentContextForProcess()->outgoing()->ServeFromStartupInfo();

  // Run until the process is killed with CTRL-C or the connections to Web
  // Engine interfaces are dropped.
  run_loop.Run();

  return 0;
}
