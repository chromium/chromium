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
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "components/fuchsia_component_support/feedback_registration.h"
#include "fuchsia_web/common/string_util.h"
#include "fuchsia_web/webengine/features.h"
#include "fuchsia_web/webengine/switches.h"
#include "fuchsia_web/webinstance_host/fuchsia_web_debug_proxy.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_finch_features.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/ozone/public/ozone_switches.h"

#if BUILDFLAG(ENABLE_WIDEVINE) && BUILDFLAG(ENABLE_CAST_RECEIVER)
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
#endif

namespace {

// Production URL for web hosting Component instances.
// The URL cannot be obtained programmatically - see fxbug.dev/51490.
const char kWebInstanceComponentUrl[] =
    "fuchsia-pkg://fuchsia.com/web_engine#meta/web_instance.cmx";

// Test-only URL for web hosting Component instances with WebUI resources.
const char kWebInstanceWithWebUiComponentUrl[] =
    "fuchsia-pkg://fuchsia.com/web_engine_with_webui#meta/web_instance.cmx";

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
// Use a constexpr instead of the existing base::Feature, because of the
// additional dependencies required.
constexpr char kMixedContentAutoupgradeFeatureName[] =
    "AutoupgradeMixedContent";
constexpr char kDisableMixedContentAutoupgradeOrigin[] =
    "disable-mixed-content-autoupgrade";
#endif

// Use a constexpr instead of the existing switch, because of the additional
// dependencies required.

// Content switches:
constexpr char kRemoteDebuggingPortSwitch[] = "remote-debugging-port";
constexpr char kDisableAcceleratedVideoDecodeSwitch[] =
    "disable-accelerated-video-decode";
constexpr char kDisableAudioInputSwitch[] = "disable-audio-input";
constexpr char kDisableAudioOutputSwitch[] = "disable-audio-output";

// Media switches:
constexpr char kDisableGpuSwitch[] = "disable-gpu";
constexpr char kDisableSoftwareRasterizerSwitch[] =
    "disable-software-rasterizer";

#if BUILDFLAG(ENABLE_WIDEVINE) && BUILDFLAG(ENABLE_CAST_RECEIVER)
// Use a constexpr instead of the media::IsClearKey() helper, because of the
// additional dependencies required.
constexpr char kClearKeyKeySystem[] = "org.w3.clearkey";
#endif

// Registers product data for the web_instance Component, ensuring it is
// registered regardless of how the Component is launched and without requiring
// all of its clients to provide the required services (until a better solution
// is available - see crbug.com/1211174). This should only be called once per
// process, and the calling thread must have an async_dispatcher.
void RegisterWebInstanceProductData() {
  // LINT.IfChange(web_engine_crash_product_name)
  static constexpr char kCrashProductName[] = "FuchsiaWebEngine";
  // LINT.ThenChange(//fuchsia_web/webengine/context_provider_main.cc:web_engine_crash_product_name)

  static constexpr char kFeedbackAnnotationsNamespace[] = "web-engine";

  fuchsia_component_support::RegisterProductDataForCrashReporting(
      kWebInstanceComponentUrl, kCrashProductName);

  fuchsia_component_support::RegisterProductDataForFeedback(
      kFeedbackAnnotationsNamespace);
}

// Appends |value| to the value of |switch_name| in the |command_line|.
// The switch is assumed to consist of comma-separated values. If |switch_name|
// is already set in |command_line| then a comma will be appended, followed by
// |value|, otherwise the switch will be set to |value|.
void AppendToSwitch(base::StringPiece switch_name,
                    base::StringPiece value,
                    base::CommandLine& command_line) {
  if (!command_line.HasSwitch(switch_name)) {
    command_line.AppendSwitchNative(switch_name, value);
    return;
  }

  std::string new_value =
      base::StrCat({command_line.GetSwitchValueASCII(switch_name), ",", value});
  command_line.RemoveSwitch(switch_name);
  command_line.AppendSwitchNative(switch_name, new_value);
}

// File names must not contain directory separators, nor match the special
// current- nor parent-directory filenames.
bool IsValidContentDirectoryName(base::StringPiece file_name) {
  if (file_name.find_first_of(base::FilePath::kSeparators, 0,
                              base::FilePath::kSeparatorsLength - 1) !=
      base::StringPiece::npos) {
    return false;
  }
  if (file_name == base::FilePath::kCurrentDirectory ||
      file_name == base::FilePath::kParentDirectory) {
    return false;
  }
  return true;
}

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

bool HandleUserAgentParams(const fuchsia::web::CreateContextParams& params,
                           base::CommandLine& launch_args) {
  if (!params.has_user_agent_product()) {
    if (params.has_user_agent_version()) {
      LOG(ERROR) << "Embedder version without product.";
      return false;
    }
    return true;
  }

  if (!net::HttpUtil::IsToken(params.user_agent_product())) {
    LOG(ERROR) << "Invalid embedder product.";
    return false;
  }

  std::string product_and_version(params.user_agent_product());
  if (params.has_user_agent_version()) {
    if (!net::HttpUtil::IsToken(params.user_agent_version())) {
      LOG(ERROR) << "Invalid embedder version.";
      return false;
    }
    base::StrAppend(&product_and_version, {"/", params.user_agent_version()});
  }
  launch_args.AppendSwitchNative(switches::kUserAgentProductAndVersion,
                                 std::move(product_and_version));
  return true;
}

void HandleUnsafelyTreatInsecureOriginsAsSecureParam(
    const fuchsia::web::CreateContextParams& params,
    base::CommandLine& launch_args) {
  if (!params.has_unsafely_treat_insecure_origins_as_secure())
    return;

  const std::vector<std::string>& insecure_origins =
      params.unsafely_treat_insecure_origins_as_secure();
  for (auto origin : insecure_origins) {
#if BUILDFLAG(ENABLE_CAST_RECEIVER)
    if (origin == switches::kAllowRunningInsecureContent) {
      launch_args.AppendSwitch(switches::kAllowRunningInsecureContent);
      continue;
    }
    if (origin == kDisableMixedContentAutoupgradeOrigin) {
      AppendToSwitch(switches::kDisableFeatures,
                     kMixedContentAutoupgradeFeatureName, launch_args);
      continue;
    }
#endif

    // Pass the list to the Context process.
    AppendToSwitch(network::switches::kUnsafelyTreatInsecureOriginAsSecure,
                   origin, launch_args);
  }
}

void HandleCorsExemptHeadersParam(
    const fuchsia::web::CreateContextParams& params,
    base::CommandLine& launch_args) {
  if (!params.has_cors_exempt_headers())
    return;

  std::vector<base::StringPiece> cors_exempt_headers;
  cors_exempt_headers.reserve(params.cors_exempt_headers().size());
  for (const auto& header : params.cors_exempt_headers()) {
    cors_exempt_headers.push_back(BytesAsString(header));
  }

  launch_args.AppendSwitchNative(switches::kCorsExemptHeaders,
                                 base::JoinString(cors_exempt_headers, ","));
}

void HandleDisableCodeGenerationParam(
    fuchsia::web::ContextFeatureFlags features,
    base::CommandLine& launch_args) {
  if ((features &
       fuchsia::web::ContextFeatureFlags::DISABLE_DYNAMIC_CODE_GENERATION) !=
      fuchsia::web::ContextFeatureFlags::DISABLE_DYNAMIC_CODE_GENERATION) {
    return;
  }

  // These flag constants must match the values defined in Blink and V8,
  // respectively. They are duplicated here rather than creating dependencies
  // of `WebInstanceHost` uses on those sub-projects.
  static constexpr char kJavaScriptFlags[] = "js-flags";
  static constexpr char kV8JitlessFlag[] = "--jitless";

  // Add the JIT-less option to the comma-separated set of V8 flags passed to
  // Blink.
  AppendToSwitch(kJavaScriptFlags, kV8JitlessFlag, launch_args);

  // TODO(crbug.com/1290907): Disable use of VmexResource in this case, once
  // migrated off of ambient VMEX.
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

bool HandleKeyboardFeatureFlags(fuchsia::web::ContextFeatureFlags features,
                                base::CommandLine& launch_args) {
  const bool enable_keyboard =
      (features & fuchsia::web::ContextFeatureFlags::KEYBOARD) ==
      fuchsia::web::ContextFeatureFlags::KEYBOARD;
  const bool enable_virtual_keyboard =
      (features & fuchsia::web::ContextFeatureFlags::VIRTUAL_KEYBOARD) ==
      fuchsia::web::ContextFeatureFlags::VIRTUAL_KEYBOARD;

  if (enable_keyboard) {
    AppendToSwitch(switches::kEnableFeatures, features::kKeyboardInput.name,
                   launch_args);

    if (enable_virtual_keyboard) {
      AppendToSwitch(switches::kEnableFeatures, features::kVirtualKeyboard.name,
                     launch_args);
    }
  } else if (enable_virtual_keyboard) {
    LOG(ERROR) << "VIRTUAL_KEYBOARD feature requires KEYBOARD.";
    return false;
  }

  return true;
}

// Returns true if DRM is supported in current configuration. Currently we
// assume that it is supported on ARM64, but not on x64.
//
// TODO(crbug.com/1013412): Detect support for all features required for
// FuchsiaCdm. Specifically we need to verify that protected memory is supported
// and that mediacodec API provides hardware video decoders.
bool IsFuchsiaCdmSupported() {
#if BUILDFLAG(ENABLE_WIDEVINE) && defined(ARCH_CPU_ARM64)
  return true;
#else
  return false;
#endif
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
  RegisterWebInstanceProductData();
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

  // Remove this argument, if it's provided.
  launch_args.RemoveSwitch(switches::kContextProvider);
  launch_args.RemoveSwitch(switches::kEnableCfv2);

  fuchsia::sys::LaunchInfo launch_info;
  // TODO(1010222): Make kWebInstanceComponentUrl a relative component URL, and
  // remove this workaround.
  launch_info.url =
      base::CommandLine::ForCurrentProcess()->HasSwitch("with-webui")
          ? kWebInstanceWithWebUiComponentUrl
          : kWebInstanceComponentUrl;
  launch_info.flat_namespace = fuchsia::sys::FlatNamespace::New();

  fuchsia::web::ContextFeatureFlags features = {};
  if (params.has_features())
    features = params.features();

  if (params.has_remote_debugging_port()) {
    if ((features & fuchsia::web::ContextFeatureFlags::NETWORK) !=
        fuchsia::web::ContextFeatureFlags::NETWORK) {
      LOG(ERROR) << "Enabling remote debugging port requires NETWORK feature.";
      return ZX_ERR_INVALID_ARGS;
    }
    launch_args.AppendSwitchNative(
        kRemoteDebuggingPortSwitch,
        base::NumberToString(params.remote_debugging_port()));
  }

  const bool is_headless =
      (features & fuchsia::web::ContextFeatureFlags::HEADLESS) ==
      fuchsia::web::ContextFeatureFlags::HEADLESS;
  if (is_headless) {
    launch_args.AppendSwitchNative(switches::kOzonePlatform,
                                   switches::kHeadless);
    launch_args.AppendSwitch(switches::kHeadless);
  }

  if ((features & fuchsia::web::ContextFeatureFlags::LEGACYMETRICS) ==
      fuchsia::web::ContextFeatureFlags::LEGACYMETRICS) {
#if BUILDFLAG(ENABLE_CAST_RECEIVER)
    launch_args.AppendSwitch(switches::kUseLegacyMetricsService);
#else
    LOG(WARNING) << "LEGACYMETRICS is not supported.";
#endif
  }

  const bool enable_vulkan =
      (features & fuchsia::web::ContextFeatureFlags::VULKAN) ==
      fuchsia::web::ContextFeatureFlags::VULKAN;
  bool enable_widevine =
      (features & fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM) ==
      fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM;
  bool enable_playready = params.has_playready_key_system();

  // Verify that the configuration is compatible with DRM, if requested.
  if (enable_widevine || enable_playready) {
    // VULKAN is required for DRM-protected video playback. Allow DRM to also be
    // enabled for HEADLESS Contexts, since Vulkan is never required for audio.
    if (!enable_vulkan && !is_headless) {
      LOG(ERROR) << "WIDEVINE_CDM and PLAYREADY_CDM features require VULKAN "
                    " or HEADLESS.";
      return ZX_ERR_NOT_SUPPORTED;
    }
    if (!params.has_cdm_data_directory()) {
      LOG(ERROR) << "WIDEVINE_CDM and PLAYREADY_CDM features require a "
                    "|cdm_data_directory|.";
      return ZX_ERR_NOT_SUPPORTED;
    }
    // |cdm_data_directory| will be handled later.
  }

  // If the system doesn't actually support DRM then disable it. This may result
  // in the Context being able to run without using protected buffers.
  if (enable_playready && !IsFuchsiaCdmSupported()) {
    LOG(WARNING) << "PlayReady is not supported on this device.";
    enable_playready = false;
  }
  if (enable_widevine && !IsFuchsiaCdmSupported()) {
    LOG(WARNING) << "Widevine is not supported on this device.";
    enable_widevine = false;
  }

  if (enable_vulkan) {
    if (is_headless) {
      DLOG(ERROR) << "VULKAN and HEADLESS features cannot be used together.";
      return ZX_ERR_NOT_SUPPORTED;
    }

    VLOG(1) << "Enabling Vulkan GPU acceleration.";

    // Vulkan requires use of SkiaRenderer, configured to a use Vulkan context.
    launch_args.AppendSwitch(switches::kUseVulkan);
    AppendToSwitch(switches::kEnableFeatures, features::kVulkan.name,
                   launch_args);
    launch_args.AppendSwitchASCII(switches::kUseGL,
                                  gl::kGLImplementationANGLEName);
  } else {
    VLOG(1) << "Disabling GPU acceleration.";
    // Disable use of Vulkan GPU, and use of the software-GL rasterizer. The
    // Context will still run a GPU process, but will not support WebGL.
    launch_args.AppendSwitch(kDisableGpuSwitch);
    launch_args.AppendSwitch(kDisableSoftwareRasterizerSwitch);
  }

#if BUILDFLAG(ENABLE_WIDEVINE)
  if (enable_widevine) {
    launch_args.AppendSwitch(switches::kEnableWidevine);
  }

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  if (enable_playready) {
    const std::string& key_system = params.playready_key_system();
    if (key_system == kWidevineKeySystem || key_system == kClearKeyKeySystem) {
      LOG(ERROR)
          << "Invalid value for CreateContextParams/playready_key_system: "
          << key_system;
      return ZX_ERR_INVALID_ARGS;
    }
    launch_args.AppendSwitchNative(switches::kPlayreadyKeySystem, key_system);
  }
#endif  // BUILDFLAG(ENABLE_CAST_RECEIVER)
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

  bool enable_audio = (features & fuchsia::web::ContextFeatureFlags::AUDIO) ==
                      fuchsia::web::ContextFeatureFlags::AUDIO;
  if (!enable_audio) {
    // TODO(fxbug.dev/58902): Split up audio input and output in
    // ContextFeatureFlags.
    launch_args.AppendSwitch(kDisableAudioOutputSwitch);
    launch_args.AppendSwitch(kDisableAudioInputSwitch);
  }

  bool enable_hardware_video_decoder =
      (features & fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER) ==
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER;
  if (!enable_hardware_video_decoder)
    launch_args.AppendSwitch(kDisableAcceleratedVideoDecodeSwitch);

  if (enable_hardware_video_decoder && !enable_vulkan) {
    DLOG(ERROR) << "HARDWARE_VIDEO_DECODER requires VULKAN.";
    return ZX_ERR_NOT_SUPPORTED;
  }

  bool disable_software_video_decoder =
      (features &
       fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER_ONLY) ==
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER_ONLY;
  if (disable_software_video_decoder) {
    if (!enable_hardware_video_decoder) {
      LOG(ERROR) << "Software video decoding may only be disabled if hardware "
                    "video decoding is enabled.";
      return ZX_ERR_INVALID_ARGS;
    }

    AppendToSwitch(switches::kDisableFeatures,
                   features::kEnableSoftwareOnlyVideoCodecs.name, launch_args);
  }

  if (!HandleCdmDataDirectoryParam(params, launch_args, launch_info)) {
    return ZX_ERR_INVALID_ARGS;
  }
  if (!HandleDataDirectoryParam(params, launch_args, launch_info)) {
    return ZX_ERR_INVALID_ARGS;
  }
  if (!HandleContentDirectoriesParam(params, launch_args, launch_info)) {
    return ZX_ERR_INVALID_ARGS;
  }
  if (!HandleUserAgentParams(params, launch_args)) {
    return ZX_ERR_INVALID_ARGS;
  }
  if (!HandleKeyboardFeatureFlags(features, launch_args)) {
    return ZX_ERR_INVALID_ARGS;
  }

  HandleUnsafelyTreatInsecureOriginsAsSecureParam(params, launch_args);
  HandleCorsExemptHeadersParam(params, launch_args);
  HandleDisableCodeGenerationParam(features, launch_args);

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
