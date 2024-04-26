// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webinstance_host/web_instance_host_internal.h"

#include <fuchsia/web/cpp/fidl.h>

#include <string_view>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "components/fuchsia_component_support/feedback_registration.h"
#include "fuchsia_web/common/string_util.h"
#include "fuchsia_web/webengine/features.h"
#include "fuchsia_web/webengine/switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_finch_features.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/network_switches.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/ozone/public/ozone_switches.h"

#if BUILDFLAG(ENABLE_WIDEVINE) && BUILDFLAG(ENABLE_CAST_RECEIVER)
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
#endif

namespace {

// Returns true if DRM is supported in current configuration. Currently we
// assume that it is supported on ARM64, but not on x64.
//
// TODO(crbug.com/42050020): Detect support for all features required for
// FuchsiaCdm. Specifically we need to verify that protected memory is supported
// and that mediacodec API provides hardware video decoders.
bool IsFuchsiaCdmSupported() {
#if BUILDFLAG(ENABLE_WIDEVINE) && defined(ARCH_CPU_ARM64)
  return true;
#else
  return false;
#endif
}

// Appends |value| to the value of |switch_name| in the |command_line|.
// The switch is assumed to consist of comma-separated values. If |switch_name|
// is already set in |command_line| then a comma will be appended, followed by
// |value|, otherwise the switch will be set to |value|.
void AppendToSwitch(std::string_view switch_name,
                    std::string_view value,
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

bool HandleUserAgentParams(fuchsia::web::CreateContextParams& params,
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

void HandleUnsafelyTreatInsecureOriginsAsSecureParam(
    fuchsia::web::CreateContextParams& params,
    base::CommandLine& launch_args) {
  if (!params.has_unsafely_treat_insecure_origins_as_secure()) {
    return;
  }

  const std::vector<std::string>& insecure_origins =
      params.unsafely_treat_insecure_origins_as_secure();
  for (const auto& origin : insecure_origins) {
#if BUILDFLAG(ENABLE_CAST_RECEIVER)
    static constexpr char kDisableMixedContentAutoupgradeOrigin[] =
        "disable-mixed-content-autoupgrade";

    if (origin == switches::kAllowRunningInsecureContent) {
      launch_args.AppendSwitch(switches::kAllowRunningInsecureContent);
      continue;
    }
    if (origin == kDisableMixedContentAutoupgradeOrigin) {
      // Constant from //third_party/blink/common/features.cc:
      static constexpr char kMixedContentAutoupgradeFeatureName[] =
          "AutoupgradeMixedContent";
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

void HandleCorsExemptHeadersParam(fuchsia::web::CreateContextParams& params,
                                  base::CommandLine& launch_args) {
  if (!params.has_cors_exempt_headers()) {
    return;
  }

  std::vector<std::string_view> cors_exempt_headers;
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

  // TODO(crbug.com/42050417): Disable use of VmexResource in this case, once
  // migrated off of ambient VMEX.
}

}  // namespace

void RegisterWebInstanceProductData(std::string_view absolute_component_url) {
  // LINT.IfChange(web_engine_crash_product_name)
  static constexpr char kCrashProductName[] = "FuchsiaWebEngine";
  // LINT.ThenChange(//fuchsia_web/webengine/context_provider_main.cc:web_engine_crash_product_name)

  static constexpr char kFeedbackAnnotationsNamespace[] = "web-engine";

  fuchsia_component_support::RegisterProductDataForCrashReporting(
      absolute_component_url, kCrashProductName);

  fuchsia_component_support::RegisterProductDataForFeedback(
      kFeedbackAnnotationsNamespace);
}

bool IsValidContentDirectoryName(std::string_view file_name) {
  if (file_name.find_first_of(base::FilePath::kSeparators, 0,
                              base::FilePath::kSeparatorsLength - 1) !=
      std::string_view::npos) {
    return false;
  }
  if (file_name == base::FilePath::kCurrentDirectory ||
      file_name == base::FilePath::kParentDirectory) {
    return false;
  }
  return true;
}

zx_status_t AppendLaunchArgs(fuchsia::web::CreateContextParams& params,
                             base::CommandLine& launch_args) {
  // Arguments to be stripped rather than propagated.
  launch_args.RemoveSwitch(switches::kContextProvider);

  const fuchsia::web::ContextFeatureFlags features =
      params.has_features() ? params.features()
                            : fuchsia::web::ContextFeatureFlags();

  if (params.has_remote_debugging_port()) {
    if ((features & fuchsia::web::ContextFeatureFlags::NETWORK) !=
        fuchsia::web::ContextFeatureFlags::NETWORK) {
      LOG(ERROR) << "Enabling remote debugging port requires NETWORK feature.";
      return ZX_ERR_INVALID_ARGS;
    }
    // Constant copied from //content/public/common/content_switches.cc:
    static constexpr char kRemoteDebuggingPort[] = "remote-debugging-port";
    launch_args.AppendSwitchNative(
        kRemoteDebuggingPort,
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
    params.clear_playready_key_system();
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
    // Constants copied from //content/public/common/content_switches.cc:
    static constexpr char kDisableGpu[] = "disable-gpu";
    static constexpr char kDisableSoftwareRasterizer[] =
        "disable-software-rasterizer";

    launch_args.AppendSwitch(kDisableGpu);
    launch_args.AppendSwitch(kDisableSoftwareRasterizer);
  }

#if BUILDFLAG(ENABLE_WIDEVINE)
  if (enable_widevine) {
    launch_args.AppendSwitch(switches::kEnableWidevine);
  }

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  // Use a constexpr instead of the media::IsClearKey() helper, because of the
  // additional dependencies required.
  static constexpr char kClearKeyKeySystem[] = "org.w3.clearkey";

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
    // Constants copied from //media/base/media_switches.cc:
    static constexpr char kDisableAudioInput[] = "disable-audio-input";
    static constexpr char kDisableAudioOutput[] = "disable-audio-output";

    launch_args.AppendSwitch(kDisableAudioOutput);
    launch_args.AppendSwitch(kDisableAudioInput);
  }

  bool enable_hardware_video_decoder =
      (features & fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER) ==
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER;
  if (!enable_hardware_video_decoder) {
    // Constant copied from //content/public/common/content_switches.cc:
    static constexpr char kDisableAcceleratedVideoDecode[] =
        "disable-accelerated-video-decode";
    launch_args.AppendSwitch(kDisableAcceleratedVideoDecode);
  }

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

  if (!HandleUserAgentParams(params, launch_args)) {
    return ZX_ERR_INVALID_ARGS;
  }

  if (!HandleKeyboardFeatureFlags(features, launch_args)) {
    return ZX_ERR_INVALID_ARGS;
  }

  HandleUnsafelyTreatInsecureOriginsAsSecureParam(params, launch_args);

  HandleCorsExemptHeadersParam(params, launch_args);

  HandleDisableCodeGenerationParam(features, launch_args);

  return ZX_OK;
}

void AppendDynamicServices(fuchsia::web::ContextFeatureFlags features,
                           bool enable_playready,
                           std::vector<std::string>& services) {
  using ::fuchsia::web::ContextFeatureFlags;

  // Result of bitwise AND when no specified flag(s) are present.
  const ContextFeatureFlags kNoFeaturesRequested =
      static_cast<ContextFeatureFlags>(0);

  // Features are listed here in order of their enum value.
  static constexpr struct {
    ContextFeatureFlags flag;
    ContextFeatureFlags value;
    std::string_view service;
  } kServices[] = {
    {ContextFeatureFlags::NETWORK, ContextFeatureFlags::NETWORK,
     "fuchsia.net.interfaces.State"},
    {ContextFeatureFlags::NETWORK, ContextFeatureFlags::NETWORK,
     "fuchsia.net.name.Lookup"},
    {ContextFeatureFlags::NETWORK, ContextFeatureFlags::NETWORK,
     "fuchsia.posix.socket.Provider"},
    {ContextFeatureFlags::AUDIO, ContextFeatureFlags::AUDIO,
     "fuchsia.media.Audio"},
    {ContextFeatureFlags::AUDIO, ContextFeatureFlags::AUDIO,
     "fuchsia.media.AudioDeviceEnumerator"},
    {ContextFeatureFlags::AUDIO, ContextFeatureFlags::AUDIO,
     "fuchsia.media.SessionAudioConsumerFactory"},
    {ContextFeatureFlags::VULKAN, ContextFeatureFlags::VULKAN,
     "fuchsia.tracing.provider.Registry"},
    {ContextFeatureFlags::VULKAN, ContextFeatureFlags::VULKAN,
     "fuchsia.vulkan.loader.Loader"},
    {ContextFeatureFlags::HARDWARE_VIDEO_DECODER,
     ContextFeatureFlags::HARDWARE_VIDEO_DECODER,
     "fuchsia.mediacodec.CodecFactory"},
  // HARDWARE_VIDEO_DECODER_ONLY does not require any additional services.
#if BUILDFLAG(ENABLE_WIDEVINE)
    {ContextFeatureFlags::WIDEVINE_CDM, ContextFeatureFlags::WIDEVINE_CDM,
     "fuchsia.media.drm.Widevine"},
#endif
    {ContextFeatureFlags::HEADLESS, kNoFeaturesRequested,
     "fuchsia.accessibility.semantics.SemanticsManager"},
    {ContextFeatureFlags::HEADLESS, kNoFeaturesRequested,
     "fuchsia.ui.composition.Allocator"},
    {ContextFeatureFlags::HEADLESS, kNoFeaturesRequested,
     "fuchsia.ui.composition.Flatland"},
#if BUILDFLAG(ENABLE_CAST_RECEIVER)
    {ContextFeatureFlags::LEGACYMETRICS, ContextFeatureFlags::LEGACYMETRICS,
     "fuchsia.legacymetrics.MetricsRecorder"},
#endif
    {ContextFeatureFlags::KEYBOARD, ContextFeatureFlags::KEYBOARD,
     "fuchsia.ui.input3.Keyboard"},
    {ContextFeatureFlags::VIRTUAL_KEYBOARD,
     ContextFeatureFlags::VIRTUAL_KEYBOARD,
     "fuchsia.input.virtualkeyboard.ControllerCreator"},
    {ContextFeatureFlags::DISABLE_DYNAMIC_CODE_GENERATION, kNoFeaturesRequested,
     "fuchsia.kernel.VmexResource"},
  };
  for (const auto& [flag, value, service] : kServices) {
    if ((features & flag) == value) {
      services.push_back(std::string(service));
    }
  }

#if BUILDFLAG(ENABLE_WIDEVINE) && BUILDFLAG(ENABLE_CAST_RECEIVER)
  if (enable_playready) {
    services.emplace_back("fuchsia.media.drm.PlayReady");
  }
#endif
}
