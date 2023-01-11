// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webinstance_host/web_instance_host_v1.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/async/default.h>
#include <lib/fdio/directory.h>
#include <lib/fdio/fd.h>
#include <lib/fdio/io.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <zircon/processargs.h>

#include <string>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/chromecast_buildflags.h"
#include "components/fuchsia_component_support/feedback_registration.h"
#include "fuchsia_web/webengine/switches.h"
#include "fuchsia_web/webinstance_host/fuchsia_web_debug_proxy.h"
#include "fuchsia_web/webinstance_host/web_instance_host_internal.h"
#include "third_party/widevine/cdm/buildflags.h"

namespace {

// Production URL for web hosting Component instances.
// The URL cannot be obtained programmatically - see fxbug.dev/51490.
const char kWebInstanceComponentUrl[] =
    "fuchsia-pkg://fuchsia.com/web_engine#meta/web_instance.cmx";

// Test-only URL for web hosting Component instances with WebUI resources.
const char kWebInstanceWithWebUiComponentUrl[] =
    "fuchsia-pkg://fuchsia.com/web_engine_with_webui#meta/web_instance.cmx";

bool HandleDataDirectoryParam(fuchsia::web::CreateContextParams& params,
                              base::CommandLine& launch_args,
                              fuchsia::sys::LaunchInfo& launch_info) {
  if (!params.has_data_directory()) {
    // Caller requested a web instance without any peristence.
    launch_args.AppendSwitch(switches::kIncognito);
    return true;
  }

  launch_info.flat_namespace->paths.push_back(
      base::kPersistedDataDirectoryPath);
  launch_info.flat_namespace->directories.push_back(
      std::move(*params.mutable_data_directory()));
  if (params.has_data_quota_bytes()) {
    launch_args.AppendSwitchNative(
        switches::kDataQuotaBytes,
        base::NumberToString(params.data_quota_bytes()));
  }

  return true;
}

bool HandleCdmDataDirectoryParam(fuchsia::web::CreateContextParams& params,
                                 base::CommandLine& launch_args,
                                 fuchsia::sys::LaunchInfo& launch_info) {
  if (!params.has_cdm_data_directory())
    return true;

  const char kCdmDataPath[] = "/cdm_data";

  launch_args.AppendSwitchNative(switches::kCdmDataDirectory, kCdmDataPath);
  launch_info.flat_namespace->paths.push_back(kCdmDataPath);
  launch_info.flat_namespace->directories.push_back(
      std::move(*params.mutable_cdm_data_directory()));
  if (params.has_cdm_data_quota_bytes()) {
    launch_args.AppendSwitchNative(
        switches::kCdmDataQuotaBytes,
        base::NumberToString(params.cdm_data_quota_bytes()));
  }

  return true;
}

bool HandleContentDirectoriesParam(fuchsia::web::CreateContextParams& params,
                                   base::CommandLine& launch_args,
                                   fuchsia::sys::LaunchInfo& launch_info) {
  DCHECK(launch_info.flat_namespace);

  if (!params.has_content_directories())
    return true;

  auto* directories = params.mutable_content_directories();
  for (size_t i = 0; i < directories->size(); ++i) {
    fuchsia::web::ContentDirectoryProvider& directory = directories->at(i);

    if (!IsValidContentDirectoryName(directory.name())) {
      DLOG(ERROR) << "Invalid directory name: " << directory.name();
      return false;
    }

    const base::FilePath kContentDirectories("/content-directories");
    launch_info.flat_namespace->paths.push_back(
        kContentDirectories.Append(directory.name()).value());
    launch_info.flat_namespace->directories.push_back(
        std::move(*directory.mutable_directory()));
  }

  launch_args.AppendSwitch(switches::kEnableContentDirectories);

  return true;
}

// Returns the names of all services required by a web_instance.cmx component
// instance configured with the specified set of feature flags. The caller is
// responsible for verifying that |params| specifies a valid combination of
// settings, before calling this function.
std::vector<std::string> GetRequiredServicesForConfig(
    const fuchsia::web::CreateContextParams& params) {
  // All web_instance.cmx instances require a common set of services, described
  // at:
  //   https://fuchsia.dev/reference/fidl/fuchsia.web#CreateContextParams.service_directory
  std::vector<std::string> services{
      "fuchsia.buildinfo.Provider",
      "fuchsia.device.NameProvider",
      "fuchsia.fonts.Provider",
      "fuchsia.hwinfo.Product",
      "fuchsia.intl.PropertyProvider",
      "fuchsia.kernel.VmexResource",
      "fuchsia.logger.LogSink",
      "fuchsia.memorypressure.Provider",
      "fuchsia.process.Launcher",
      "fuchsia.settings.Display",  // Used if preferred theme is DEFAULT.
      "fuchsia.sysmem.Allocator",
      "fuchsia.tracing.perfetto.ProducerConnector",
      "fuchsia.tracing.provider.Registry",
      "fuchsia.ui.scenic.Scenic"};

  // TODO(crbug.com/1209031): Provide these conditionally, once corresponding
  // ContextFeatureFlags have been defined.
  services.insert(services.end(), {"fuchsia.camera3.DeviceWatcher",
                                   "fuchsia.media.ProfileProvider"});

  // Additional services are required depending on particular configuration
  // parameters.

  // Additional services are required dependent on the set of features specified
  // for the instance, as described at:
  //   https://fuchsia.dev/reference/fidl/fuchsia.web#ContextFeatureFlags
  // Features are listed here in order of their enum value.
  fuchsia::web::ContextFeatureFlags features = {};
  if (params.has_features())
    features = params.features();

  if ((features & fuchsia::web::ContextFeatureFlags::NETWORK) ==
      fuchsia::web::ContextFeatureFlags::NETWORK) {
    services.insert(services.end(), {
                                        "fuchsia.net.interfaces.State",
                                        "fuchsia.net.name.Lookup",
                                        "fuchsia.posix.socket.Provider",
                                    });
  }

  if ((features & fuchsia::web::ContextFeatureFlags::AUDIO) ==
      fuchsia::web::ContextFeatureFlags::AUDIO) {
    services.insert(services.end(),
                    {
                        "fuchsia.media.Audio",
                        "fuchsia.media.AudioDeviceEnumerator",
                        "fuchsia.media.SessionAudioConsumerFactory",
                    });
  }

  if ((features & fuchsia::web::ContextFeatureFlags::VULKAN) ==
      fuchsia::web::ContextFeatureFlags::VULKAN) {
    services.emplace_back("fuchsia.vulkan.loader.Loader");
  }

  if ((features & fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER) ==
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER) {
    services.emplace_back("fuchsia.mediacodec.CodecFactory");
  }

  // HARDWARE_VIDEO_DECODER_ONLY does not require any additional services.

#if BUILDFLAG(ENABLE_WIDEVINE)
  if ((features & fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM) ==
      fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM) {
    services.emplace_back("fuchsia.media.drm.Widevine");
  }

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  if (params.has_playready_key_system()) {
    services.emplace_back("fuchsia.media.drm.PlayReady");
  }
#endif  // BUILDFLAG(ENABLE_CAST_RECEIVER)
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

  // HEADLESS instances cannot create Views and therefore do not require access
  // to any View-based services.
  if ((features & fuchsia::web::ContextFeatureFlags::HEADLESS) !=
      fuchsia::web::ContextFeatureFlags::HEADLESS) {
    services.insert(services.end(),
                    {
                        "fuchsia.accessibility.semantics.SemanticsManager",
                        "fuchsia.ui.composition.Allocator",
                        "fuchsia.ui.composition.Flatland",
                        "fuchsia.ui.scenic.Scenic",
                    });
  }

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  if ((features & fuchsia::web::ContextFeatureFlags::LEGACYMETRICS) ==
      fuchsia::web::ContextFeatureFlags::LEGACYMETRICS) {
    services.emplace_back("fuchsia.legacymetrics.MetricsRecorder");
  }
#endif

  if ((features & fuchsia::web::ContextFeatureFlags::KEYBOARD) ==
      fuchsia::web::ContextFeatureFlags::KEYBOARD) {
    services.emplace_back("fuchsia.ui.input3.Keyboard");
  }

  if ((features & fuchsia::web::ContextFeatureFlags::VIRTUAL_KEYBOARD) ==
      fuchsia::web::ContextFeatureFlags::VIRTUAL_KEYBOARD) {
    services.emplace_back("fuchsia.input.virtualkeyboard.ControllerCreator");
  }

  return services;
}

}  // namespace

WebInstanceHostV1::WebInstanceHostV1() {
  // Ensure WebInstance is registered before launching it.
  // TODO(crbug.com/1211174): Replace with a different mechanism when available.
  RegisterWebInstanceProductData(kWebInstanceComponentUrl);
}

WebInstanceHostV1::~WebInstanceHostV1() = default;

zx_status_t WebInstanceHostV1::CreateInstanceForContextWithCopiedArgs(
    fuchsia::web::CreateContextParams params,
    fidl::InterfaceRequest<fuchsia::io::Directory> services_request,
    base::CommandLine extra_args) {
  DCHECK(services_request);

  if (!params.has_service_directory()) {
    DLOG(ERROR)
        << "Missing argument |service_directory| in CreateContextParams.";
    return ZX_ERR_INVALID_ARGS;
  }

  fidl::InterfaceHandle<::fuchsia::io::Directory> service_directory =
      std::move(*params.mutable_service_directory());
  if (!service_directory) {
    DLOG(ERROR) << "Invalid |service_directory| in CreateContextParams.";
    return ZX_ERR_INVALID_ARGS;
  }

  // Initialize with preliminary arguments.
  base::CommandLine launch_args(std::move(extra_args));
  if (zx_status_t status = AppendLaunchArgs(params, launch_args);
      status != ZX_OK) {
    return status;
  }

  fuchsia::sys::LaunchInfo launch_info;
  // TODO(1010222): Make kWebInstanceComponentUrl a relative component URL, and
  // remove this workaround.
  launch_info.url =
      base::CommandLine::ForCurrentProcess()->HasSwitch("with-webui")
          ? kWebInstanceWithWebUiComponentUrl
          : kWebInstanceComponentUrl;
  launch_info.flat_namespace = fuchsia::sys::FlatNamespace::New();

  if (!HandleCdmDataDirectoryParam(params, launch_args, launch_info)) {
    return ZX_ERR_INVALID_ARGS;
  }
  if (!HandleDataDirectoryParam(params, launch_args, launch_info)) {
    return ZX_ERR_INVALID_ARGS;
  }
  if (!HandleContentDirectoriesParam(params, launch_args, launch_info)) {
    return ZX_ERR_INVALID_ARGS;
  }

  // In tests the ContextProvider is configured to log to stderr, so clone the
  // handle to allow web instances to also log there.
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "enable-logging") == "stderr") {
    launch_info.err = fuchsia::sys::FileDescriptor::New();
    launch_info.err->type0 = PA_FD;
    zx_status_t status = fdio_fd_clone(
        STDERR_FILENO, launch_info.err->handle0.reset_and_get_address());
    ZX_CHECK(status == ZX_OK, status);
  }

  if (tmp_dir_.is_valid()) {
    launch_info.flat_namespace->paths.push_back("/tmp");
    launch_info.flat_namespace->directories.push_back(std::move(tmp_dir_));
  }

  // Create a request for the new instance's service-directory.
  fidl::InterfaceHandle<fuchsia::io::Directory> instance_services_handle;
  launch_info.directory_request = instance_services_handle.NewRequest();
  sys::ServiceDirectory instance_services(std::move(instance_services_handle));

  // If one or more Debug protocol clients are active then enable debugging,
  // and connect the instance to the fuchsia.web.Debug proxy.
  if (debug_proxy_.has_clients()) {
    launch_args.AppendSwitch(switches::kEnableRemoteDebugMode);
    fidl::InterfaceHandle<fuchsia::web::Debug> debug_handle;
    instance_services.Connect(debug_handle.NewRequest());
    debug_proxy_.RegisterInstance(std::move(debug_handle));
  }

  // Pass on the caller's service-directory request.
  instance_services.CloneChannel(std::move(services_request));

  // Set |additional_services| to redirect requests for only those services
  // required for the specified |params|, to be satisfied by the caller-
  // supplied service directory. This reduces the risk of an instance being
  // able to somehow exploit services other than those that it should be using.
  launch_info.additional_services = fuchsia::sys::ServiceList::New();
  launch_info.additional_services->names = GetRequiredServicesForConfig(params);
  launch_info.additional_services->host_directory =
      std::move(service_directory);

  // Take the accumulated command line arguments, omitting the program name in
  // argv[0], and set them in |launch_info|.
  launch_info.arguments = std::vector<std::string>(
      launch_args.argv().begin() + 1, launch_args.argv().end());

  // Launch the component with the accumulated settings.  The Component will
  // self-terminate when the fuchsia.web.Context client disconnects.
  IsolatedEnvironmentLauncher()->CreateComponent(std::move(launch_info),
                                                 nullptr /* controller */);

  return ZX_OK;
}

fuchsia::web::Debug* WebInstanceHostV1::debug_api() {
  return &debug_proxy_;
}

fuchsia::sys::Launcher* WebInstanceHostV1::IsolatedEnvironmentLauncher() {
  if (isolated_environment_launcher_)
    return isolated_environment_launcher_.get();

  // Create the nested isolated Environment. This environment provides only the
  // fuchsia.sys.Loader service, which is required to allow the Launcher to
  // resolve the web instance package. All other services are provided
  // explicitly to each web instance, from those passed to |CreateContext()|.
  auto environment = base::ComponentContextForProcess()
                         ->svc()
                         ->Connect<fuchsia::sys::Environment>();

  // Populate a ServiceList providing only the Loader service.
  auto services = fuchsia::sys::ServiceList::New();
  services->names.push_back(fuchsia::sys::Loader::Name_);
  fidl::InterfaceHandle<::fuchsia::io::Directory> services_channel;
  environment->GetDirectory(services_channel.NewRequest());
  services->host_directory = std::move(services_channel);

  // Instantiate the isolated environment. This ContextProvider instance's PID
  // is included in the label to ensure that concurrent service instances
  // launched in the same Environment (e.g. during tests) do not clash.
  fuchsia::sys::EnvironmentPtr isolated_environment;
  environment->CreateNestedEnvironment(
      isolated_environment.NewRequest(),
      isolated_environment_controller_.NewRequest(),
      base::StringPrintf("web_instances:%lu", base::Process::Current().Pid()),
      std::move(services),
      {.inherit_parent_services = false,
       .use_parent_runners = false,
       .delete_storage_on_death = true});

  // The ContextProvider only needs to retain the EnvironmentController and
  // a connection to the Launcher service for the isolated environment.
  isolated_environment->GetLauncher(
      isolated_environment_launcher_.NewRequest());
  isolated_environment_launcher_.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status) << "Launcher disconnected.";
  });
  isolated_environment_controller_.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status) << "EnvironmentController disconnected.";
  });

  return isolated_environment_launcher_.get();
}
