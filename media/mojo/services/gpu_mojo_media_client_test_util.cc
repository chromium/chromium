// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/gpu_mojo_media_client_test_util.h"

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_util.h"
#include "media/base/android_overlay_mojo_factory.h"
#include "media/base/media_types.h"
#include "media/base/supported_types.h"
#include "media/base/video_codecs.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/services/gpu_mojo_media_client.h"

namespace media {

void AddSupplementalCodecsForTesting(gpu::GpuPreferences gpu_preferences) {
#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER) || \
    BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_SUPPORT)
  const gpu::GpuDriverBugWorkarounds dummy_workarounds;
  const gpu::GPUInfo gpu_info = []() {
    gpu::GPUInfo info;
    // Deliberately not calling gpu::CollectGraphicsInfoForTesting() because it
    // just forwards the call to gpu::CollectContextGraphicsInfo() on Android.
    // gpu::CollectContextGraphicsInfo() requires an actual GL context and is
    // not really meant to be used outside of the GPU process.
    // gpu::CollectBasicGraphicsInfo() gives us what we need here anyway, on
    // all platforms.
    gpu::CollectBasicGraphicsInfo(&info);
    return info;
  }();
  const gpu::GpuFeatureInfo gpu_feature_info = gpu::ComputeGpuFeatureInfo(
      gpu_info, gpu_preferences, base::CommandLine::ForCurrentProcess(),
      /*needs_more_info=*/nullptr);
  GpuMojoMediaClientTraits traits(
      gpu_preferences, dummy_workarounds, gpu_feature_info, gpu_info,
      /*gpu_task_runner=*/nullptr, AndroidOverlayMojoFactoryCB(),
      /*media_gpu_channel_manager=*/nullptr);
  auto client = GpuMojoMediaClient::Create(traits);

#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
  base::flat_set<AudioType> supported_audio_types;
  for (const auto& config : client->GetSupportedAudioDecoderConfigs()) {
    supported_audio_types.emplace(config.codec, config.profile,
                                  /*spatial_rendering=*/false);
  }
  UpdateDefaultSupportedAudioTypes(supported_audio_types);
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)

#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_SUPPORT)
  base::flat_set<VideoCodecProfile> supported_video_profiles;
  for (const auto& config : client->GetSupportedVideoDecoderConfigs()) {
    for (int profile = config.profile_min; profile <= config.profile_max;
         ++profile) {
      supported_video_profiles.insert(static_cast<VideoCodecProfile>(profile));
    }
  }
  UpdateDefaultSupportedVideoProfiles(supported_video_profiles);
#endif  // BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_SUPPORT)
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER) || \
        // BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_SUPPORT)
}

}  // namespace media
