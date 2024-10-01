// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_config.h"

#include <string_view>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/embedder_support/switches.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "fuchsia_web/webengine/switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_switches.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/display/display_switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/ozone/public/ozone_switches.h"

namespace {

// Returns true if protected memory is supported. Currently we assume that it is
// supported on ARM64, but not on x64.
//
// TODO(crbug.com/42050020): Detect if protected memory is supported.
bool IsProtectedMemorySupported() {
#if defined(ARCH_CPU_ARM64)
  return true;
#else
  return false;
#endif
}

// Appends `value` to the value of `switch_name` in the `command_line`.
// The switch is assumed to consist of comma-separated values. If `switch_name`
// is already set in `command_line` then a comma will be appended, followed by
// `value`, otherwise the switch will be set to `value`.
void AppendToSwitch(std::string_view switch_name,
                    std::string_view value,
                    base::CommandLine* command_line,
                    std::string_view separator = ",") {
  if (!command_line->HasSwitch(switch_name)) {
    command_line->AppendSwitchNative(switch_name, value);
    return;
  }

  std::string new_value = base::StrCat(
      {command_line->GetSwitchValueASCII(switch_name), separator, value});
  command_line->RemoveSwitch(switch_name);
  command_line->AppendSwitchNative(switch_name, new_value);
}

bool AddCommandLineArgsFromConfig(const base::Value::Dict& config,
                                  base::CommandLine* command_line) {
  const base::Value::Dict* args = config.FindDict("command-line-args");
  if (!args) {
    return true;
  }

  static const std::string_view kAllowedArgs[] = {
      blink::switches::kSharedArrayBufferAllowedOrigins,
      blink::switches::kGpuRasterizationMSAASampleCount,
      blink::switches::kMinHeightForGpuRasterTile,
      cc::switches::kEnableClippedImageScaling,
      cc::switches::kEnableGpuBenchmarking,
      embedder_support::kOriginTrialPublicKey,
      embedder_support::kOriginTrialDisabledFeatures,
      switches::kDisableFeatures,
      switches::kDisableGpuWatchdog,
      switches::kDisableQuic,
      switches::kDisableMipmapGeneration,
      // TODO(crbug.com/40131115): Remove this switch from the allow-list.
      switches::kEnableCastStreamingReceiver,
      switches::kEnableFeatures,
      switches::kEnableLowEndDeviceMode,
      switches::kForceDeviceScaleFactor,
      switches::kForceGpuMemAvailableMb,
      switches::kForceGpuMemDiscardableLimitMb,
      switches::kForceMaxTextureSize,
      switches::kGoogleApiKey,
      switches::kInProcessGPU,
      switches::kMaxDecodedImageSizeMb,
      switches::kMinVideoDecoderOutputBufferSize,
      switches::kOzonePlatform,
      switches::kRendererProcessLimit,
      switches::kUseCmdDecoder,
      switches::kV,
      switches::kVModule,
      switches::kVulkanHeapMemoryLimitMb,
      switches::kVulkanSyncCpuMemoryLimitMb,
      switches::kWebglAntialiasingMode,
      switches::kWebglMSAASampleCount,
  };

  for (const auto arg : *args) {
    if (!base::Contains(kAllowedArgs, arg.first)) {
      // TODO(crbug.com/40662865): Increase severity and return false
      // once we have a mechanism for soft transitions of supported arguments.
      LOG(WARNING) << "Unknown command-line arg: '" << arg.first
                   << "'. Config file and WebEngine version may not match.";
      continue;
    }

    if (arg.first == switches::kEnableFeatures ||
        arg.first == switches::kDisableFeatures) {
      if (!arg.second.is_string()) {
        LOG(ERROR) << "Config command-line arg must be a string: " << arg.first;
        return false;
      }
      // Merge the features.
      AppendToSwitch(arg.first, arg.second.GetString(), command_line);
      continue;
    }

    if (command_line->HasSwitch(arg.first)) {
      // Use the existing command line value rather than override it.
      continue;
    }

    if (arg.second.is_none()) {
      command_line->AppendSwitch(arg.first);
      continue;
    }

    if (arg.second.is_string()) {
      command_line->AppendSwitchNative(arg.first, arg.second.GetString());
      continue;
    }

    LOG(ERROR) << "Config command-line arg must be a string: " << arg.first;
    return false;
  }

  // Disable kWebRtcHWDecoding by default until config-data are updated.
  // TODO(b/326282208): Remove once config-data are updated to use the new
  // feature.
  AppendToSwitch(switches::kDisableFeatures, features::kWebRtcHWDecoding.name,
                 command_line);

  return true;
}

}  // namespace

bool UpdateCommandLineFromConfigFile(const base::Value::Dict& config,
                                     base::CommandLine* command_line) {
  // The FieldTrialList should be initialized only after config is loaded.
  CHECK(!base::FieldTrialList::GetInstance());

  if (!AddCommandLineArgsFromConfig(config, command_line)) {
    return false;
  }

  // The following two args are set by calling component. They are used to set
  // other flags below.
  const bool playready_enabled =
      command_line->HasSwitch(switches::kPlayreadyKeySystem);
  const bool widevine_enabled =
      command_line->HasSwitch(switches::kEnableWidevine);

  // Ignore "force-protected-video-buffers" if protected memory is not
  // supported. This is necessary to workaround https://fxbug.dev/126639.
  const bool force_protected_video_buffers =
      IsProtectedMemorySupported() &&
      config.FindBool("force-protected-video-buffers").value_or(false);

  const bool enable_protected_graphics =
      playready_enabled || widevine_enabled || force_protected_video_buffers;

  if (enable_protected_graphics) {
    command_line->AppendSwitch(switches::kEnableVulkanProtectedMemory);
    command_line->AppendSwitch(switches::kEnableProtectedVideoBuffers);
  }

  if (force_protected_video_buffers) {
    command_line->AppendSwitch(switches::kForceProtectedVideoOutputBuffers);
  }

  // TODO(crbug.com/40269624): Remove this switch once fixed.
  command_line->AppendSwitchASCII(switches::kEnableHardwareOverlays,
                                  "underlay");

  std::optional<int> max_old_space =
      config.FindInt("js-heap-max-old-space-size");
  if (max_old_space) {
    AppendToSwitch(
        blink::switches::kJavaScriptFlags,
        "--max_old_space_size=" + base::NumberToString(max_old_space.value()),
        command_line, " ");
  }

  std::optional<int> max_semi_space =
      config.FindInt("js-heap-max-semi-space-size");
  if (max_semi_space) {
    AppendToSwitch(
        blink::switches::kJavaScriptFlags,
        "--max_semi_space_size=" + base::NumberToString(max_semi_space.value()),
        command_line, " ");
  }

  return true;
}
