// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_config.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "cc/base/switches.h"
#include "components/embedder_support/switches.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "content/public/common/content_switches.h"
#include "fuchsia_web/webengine/switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_switches.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/display/display_switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/ozone/public/ozone_switches.h"

namespace {

// Appends `value` to the value of `switch_name` in the `command_line`.
// The switch is assumed to consist of comma-separated values. If `switch_name`
// is already set in `command_line` then a comma will be appended, followed by
// `value`, otherwise the switch will be set to `value`.
void AppendToSwitch(base::StringPiece switch_name,
                    base::StringPiece value,
                    base::CommandLine* command_line) {
  if (!command_line->HasSwitch(switch_name)) {
    command_line->AppendSwitchNative(switch_name, value);
    return;
  }

  std::string new_value = base::StrCat(
      {command_line->GetSwitchValueASCII(switch_name), ",", value});
  command_line->RemoveSwitch(switch_name);
  command_line->AppendSwitchNative(switch_name, new_value);
}

bool AddCommandLineArgsFromConfig(const base::Value::Dict& config,
                                  base::CommandLine* command_line) {
  const base::Value::Dict* args = config.FindDict("command-line-args");
  if (!args)
    return true;

  static const base::StringPiece kAllowedArgs[] = {
      blink::switches::kSharedArrayBufferAllowedOrigins,
      blink::switches::kGpuRasterizationMSAASampleCount,
      blink::switches::kMinHeightForGpuRasterTile,
      cc::switches::kEnableClippedImageScaling,
      cc::switches::kEnableGpuBenchmarking,
      embedder_support::kOriginTrialPublicKey,
      embedder_support::kOriginTrialDisabledFeatures,
      embedder_support::kOriginTrialDisabledTokens,
      switches::kDisableFeatures,
      switches::kDisableGpuWatchdog,
      switches::kDisableQuic,
      switches::kDisableMipmapGeneration,
      // TODO(crbug.com/1082821): Remove this switch from the allow-list.
      switches::kEnableCastStreamingReceiver,
      switches::kEnableFeatures,
      switches::kEnableLowEndDeviceMode,
      switches::kForceDeviceScaleFactor,
      switches::kForceGpuMemAvailableMb,
      switches::kForceGpuMemDiscardableLimitMb,
      switches::kForceMaxTextureSize,
      switches::kGoogleApiKey,
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
      // TODO(https://crbug.com/1032439): Increase severity and return false
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

  return true;
}

}  // namespace

bool UpdateCommandLineFromConfigFile(const base::Value::Dict& config,
                                     base::CommandLine* command_line) {
  // The FieldTrialList should be initialized only after config is loaded.
  CHECK(!base::FieldTrialList::GetInstance());

  if (!AddCommandLineArgsFromConfig(config, command_line))
    return false;

  // The following two args are set by calling component. They are used to set
  // other flags below.
  const bool playready_enabled =
      command_line->HasSwitch(switches::kPlayreadyKeySystem);
  const bool widevine_enabled =
      command_line->HasSwitch(switches::kEnableWidevine);

  const bool allow_protected_graphics =
      config.FindBool("allow-protected-graphics").value_or(false);
  const bool force_protected_graphics =
      config.FindBool("force-protected-graphics").value_or(false);
  const bool enable_protected_graphics =
      ((playready_enabled || widevine_enabled) && allow_protected_graphics) ||
      force_protected_graphics;
  const bool use_overlays_for_video =
      config.FindBool("use-overlays-for-video").value_or(false);

  if (enable_protected_graphics) {
    command_line->AppendSwitch(switches::kEnableVulkanProtectedMemory);
    command_line->AppendSwitch(switches::kEnableProtectedVideoBuffers);
    const bool force_protected_video_buffers =
        config.FindBool("force-protected-video-buffers").value_or(false);
    if (force_protected_video_buffers) {
      command_line->AppendSwitch(switches::kForceProtectedVideoOutputBuffers);
    }
  }

  if (use_overlays_for_video) {
    // Overlays are only available if OutputPresenterFuchsia is in use.
    AppendToSwitch(switches::kEnableFeatures,
                   features::kUseSkiaOutputDeviceBufferQueue.name,
                   command_line);
    AppendToSwitch(switches::kEnableFeatures,
                   features::kUseRealBuffersForPageFlipTest.name, command_line);
    command_line->AppendSwitchASCII(switches::kEnableHardwareOverlays,
                                    "underlay");
    command_line->AppendSwitch(switches::kUseOverlaysForVideo);
  }

  return true;
}
