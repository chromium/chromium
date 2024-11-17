// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/gpu_video_encode_accelerator_factory.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/macros.h"
#include "media/video/video_encode_accelerator.h"

#if BUILDFLAG(IS_ANDROID)
#include "media/gpu/android/android_video_encode_accelerator.h"
#include "media/gpu/android/ndk_video_encode_accelerator.h"
#endif
#if BUILDFLAG(IS_MAC)
#include "media/gpu/mac/vt_video_encode_accelerator_mac.h"
#endif
#if BUILDFLAG(IS_WIN)
#include "media/gpu/windows/media_foundation_video_encode_accelerator_win.h"
#endif
#if BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_video_encode_accelerator.h"
#elif BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_video_encode_accelerator.h"
#endif
#if BUILDFLAG(IS_FUCHSIA)
#include "media/fuchsia/video/fuchsia_video_encode_accelerator.h"
#endif

namespace media {

namespace {
#if BUILDFLAG(USE_V4L2_CODEC)
std::unique_ptr<VideoEncodeAccelerator> CreateV4L2VEA() {
#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/901264): Encoders use hack for passing offset within
  // a DMA-buf, which is not supported upstream.
  return base::WrapUnique<VideoEncodeAccelerator>(
      new V4L2VideoEncodeAccelerator(new V4L2Device()));
#else
  return nullptr;
#endif
}
#elif BUILDFLAG(USE_VAAPI)
std::unique_ptr<VideoEncodeAccelerator> CreateVaapiVEA() {
  return base::WrapUnique<VideoEncodeAccelerator>(
      new VaapiVideoEncodeAccelerator());
}
#endif

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<VideoEncodeAccelerator> CreateAndroidVEA() {
  if (__builtin_available(android NDK_MEDIA_CODEC_MIN_API, *)) {
    return base::WrapUnique<VideoEncodeAccelerator>(
        new NdkVideoEncodeAccelerator(
            base::SequencedTaskRunner::GetCurrentDefault()));
  } else {
    return base::WrapUnique<VideoEncodeAccelerator>(
        new AndroidVideoEncodeAccelerator());
  }
}
#endif

#if BUILDFLAG(IS_MAC)
std::unique_ptr<VideoEncodeAccelerator> CreateVTVEA() {
  return base::WrapUnique<VideoEncodeAccelerator>(
      new VTVideoEncodeAccelerator());
}
#endif

#if BUILDFLAG(IS_WIN)
std::unique_ptr<VideoEncodeAccelerator> CreateMediaFoundationVEA(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    const gpu::GPUInfo::GPUDevice& gpu_device) {
  return base::WrapUnique<VideoEncodeAccelerator>(
      new MediaFoundationVideoEncodeAccelerator(
          gpu_preferences, gpu_workarounds, gpu_device.luid));
}
#endif

#if BUILDFLAG(IS_FUCHSIA)
std::unique_ptr<VideoEncodeAccelerator> CreateFuchsiaVEA() {
  return base::WrapUnique<VideoEncodeAccelerator>(
      new FuchsiaVideoEncodeAccelerator());
}
#endif

using VEAFactoryFunction =
    base::RepeatingCallback<std::unique_ptr<VideoEncodeAccelerator>()>;

std::vector<VEAFactoryFunction> GetVEAFactoryFunctions(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    const gpu::GPUInfo::GPUDevice& gpu_device) {
  // Array of VEAFactoryFunctions potentially usable on the current platform.
  // This list is ordered by priority, from most to least preferred, if
  // applicable. This list is composed once and then reused.
  static std::vector<VEAFactoryFunction> vea_factory_functions;
  if (gpu_preferences.disable_accelerated_video_encode)
    return vea_factory_functions;
  if (!vea_factory_functions.empty())
    return vea_factory_functions;

#if BUILDFLAG(USE_VAAPI)
#if BUILDFLAG(IS_LINUX)
  if (base::FeatureList::IsEnabled(kAcceleratedVideoEncodeLinux)) {
    vea_factory_functions.push_back(base::BindRepeating(&CreateVaapiVEA));
  }
#else
  vea_factory_functions.push_back(base::BindRepeating(&CreateVaapiVEA));
#endif
#elif BUILDFLAG(USE_V4L2_CODEC)
#if BUILDFLAG(IS_LINUX)
  if (base::FeatureList::IsEnabled(kAcceleratedVideoEncodeLinux)) {
    vea_factory_functions.push_back(base::BindRepeating(&CreateV4L2VEA));
  }
#else
  vea_factory_functions.push_back(base::BindRepeating(&CreateV4L2VEA));
#endif
#endif

#if BUILDFLAG(IS_ANDROID)
  vea_factory_functions.push_back(base::BindRepeating(&CreateAndroidVEA));
#endif
#if BUILDFLAG(IS_MAC)
  vea_factory_functions.push_back(base::BindRepeating(&CreateVTVEA));
#endif
#if BUILDFLAG(IS_WIN)
  vea_factory_functions.push_back(base::BindRepeating(
      &CreateMediaFoundationVEA, gpu_preferences, gpu_workarounds, gpu_device));
#endif
#if BUILDFLAG(IS_FUCHSIA)
  if (base::FeatureList::IsEnabled(kFuchsiaMediacodecVideoEncoder)) {
    vea_factory_functions.push_back(base::BindRepeating(&CreateFuchsiaVEA));
  }
#endif
  return vea_factory_functions;
}

VideoEncodeAccelerator::SupportedProfiles GetSupportedProfilesInternal(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    const gpu::GPUInfo::GPUDevice& gpu_device) {
  if (gpu_preferences.disable_accelerated_video_encode)
    return VideoEncodeAccelerator::SupportedProfiles();

  VideoEncodeAccelerator::SupportedProfiles profiles;
  for (const auto& create_vea :
       GetVEAFactoryFunctions(gpu_preferences, gpu_workarounds, gpu_device)) {
    auto vea = std::move(create_vea).Run();
    if (!vea)
      continue;
    auto vea_profiles = vea->GetSupportedProfiles();
    GpuVideoAcceleratorUtil::InsertUniqueEncodeProfiles(vea_profiles,
                                                        &profiles);
  }
  return profiles;
}

}  // anonymous namespace

// static
MEDIA_GPU_EXPORT std::unique_ptr<VideoEncodeAccelerator>
GpuVideoEncodeAcceleratorFactory::CreateVEA(
    const VideoEncodeAccelerator::Config& config,
    VideoEncodeAccelerator::Client* client,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    const gpu::GPUInfo::GPUDevice& gpu_device,
    std::unique_ptr<MediaLog> media_log,
    GetCommandBufferHelperCB get_command_buffer_helper_cb,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner) {
  // NullMediaLog silently and safely does nothing.
  if (!media_log)
    media_log = std::make_unique<media::NullMediaLog>();

  for (const auto& create_vea :
       GetVEAFactoryFunctions(gpu_preferences, gpu_workarounds, gpu_device)) {
    std::unique_ptr<VideoEncodeAccelerator> vea = create_vea.Run();
    if (!vea)
      continue;
    if (!get_command_buffer_helper_cb.is_null()) {
      vea->SetCommandBufferHelperCB(get_command_buffer_helper_cb,
                                    gpu_task_runner);
    }
    if (!vea->Initialize(config, client, media_log->Clone())) {
      DLOG(ERROR) << "VEA initialize failed (" << config.AsHumanReadableString()
                  << ")";
      continue;
    }
    return vea;
  }
  return nullptr;
}

// static
MEDIA_GPU_EXPORT VideoEncodeAccelerator::SupportedProfiles
GpuVideoEncodeAcceleratorFactory::GetSupportedProfiles(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    const gpu::GPUInfo::GPUDevice& gpu_device) {
  // Cache the supported profiles so that they will not be computed more than
  // once per GPU process. It is assumed that |gpu_preferences| do not change
  // between calls.
  static auto profiles = GetSupportedProfilesInternal(
      gpu_preferences, gpu_workarounds, gpu_device);

#if BUILDFLAG(USE_V4L2_CODEC)
  // V4L2-only: the encoder devices may not be visible at the time the GPU
  // process is starting. If the capabilities vector is empty, try to query the
  // devices again in the hope that they will have appeared in the meantime.
  // TODO(crbug.com/948147): trigger query when an device add/remove event
  // (e.g. via udev) has happened instead.
  if (profiles.empty()) {
    VLOGF(1) << "Supported profiles empty, querying again...";
    profiles = GetSupportedProfilesInternal(gpu_preferences, gpu_workarounds, gpu_device);
  }
#endif

  if (gpu_workarounds.disable_accelerated_av1_encode) {
    std::erase_if(profiles, [](const auto& vea_profile) {
      return vea_profile.profile >= AV1PROFILE_PROFILE_MAIN &&
             vea_profile.profile <= AV1PROFILE_PROFILE_PRO;
    });
  }

  if (gpu_workarounds.disable_accelerated_vp8_encode) {
    std::erase_if(profiles, [](const auto& vea_profile) {
      return vea_profile.profile == VP8PROFILE_ANY;
    });
  }

  if (gpu_workarounds.disable_accelerated_vp9_encode) {
    std::erase_if(profiles, [](const auto& vea_profile) {
      return vea_profile.profile >= VP9PROFILE_PROFILE0 &&
             vea_profile.profile <= VP9PROFILE_PROFILE3;
    });
  }

  if (gpu_workarounds.disable_accelerated_h264_encode) {
    std::erase_if(profiles, [](const auto& vea_profile) {
      return vea_profile.profile >= H264PROFILE_MIN &&
             vea_profile.profile <= H264PROFILE_MAX;
    });
  }

  return profiles;
}

}  // namespace media
