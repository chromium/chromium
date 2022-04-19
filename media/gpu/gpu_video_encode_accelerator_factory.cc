// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/gpu_video_encode_accelerator_factory.h"

#include "base/bind.h"
#include "base/containers/cxx20_erase.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/macros.h"

#if BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_video_encode_accelerator.h"
#endif
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
#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_video_encode_accelerator.h"
#endif

namespace media {

namespace {
#if BUILDFLAG(USE_V4L2_CODEC)
std::unique_ptr<VideoEncodeAccelerator> CreateV4L2VEA() {
  scoped_refptr<V4L2Device> device = V4L2Device::Create();
  if (!device)
    return nullptr;
  return base::WrapUnique<VideoEncodeAccelerator>(
      new V4L2VideoEncodeAccelerator(std::move(device)));
}
#endif

#if BUILDFLAG(USE_VAAPI)
std::unique_ptr<VideoEncodeAccelerator> CreateVaapiVEA() {
  return base::WrapUnique<VideoEncodeAccelerator>(
      new VaapiVideoEncodeAccelerator());
}
#endif

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<VideoEncodeAccelerator> CreateAndroidVEA() {
  if (NdkVideoEncodeAccelerator::IsSupported()) {
    return base::WrapUnique<VideoEncodeAccelerator>(
        new NdkVideoEncodeAccelerator(base::ThreadTaskRunnerHandle::Get()));
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
// Creates a MediaFoundationVEA for Win 7 or later. If |compatible_with_win7| is
// true, VEA is limited to a subset of features that is compatible with Win 7.
std::unique_ptr<VideoEncodeAccelerator> CreateMediaFoundationVEA(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds) {
  return base::WrapUnique<VideoEncodeAccelerator>(
      new MediaFoundationVideoEncodeAccelerator(gpu_preferences,
                                                gpu_workarounds));
}
#endif

using VEAFactoryFunction =
    base::RepeatingCallback<std::unique_ptr<VideoEncodeAccelerator>()>;

std::vector<VEAFactoryFunction> GetVEAFactoryFunctions(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds) {
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
  if (base::FeatureList::IsEnabled(kVaapiVideoEncodeLinux))
    vea_factory_functions.push_back(base::BindRepeating(&CreateVaapiVEA));
#else
  vea_factory_functions.push_back(base::BindRepeating(&CreateVaapiVEA));
#endif
#endif
#if BUILDFLAG(USE_V4L2_CODEC)
  vea_factory_functions.push_back(base::BindRepeating(&CreateV4L2VEA));
#endif
#if BUILDFLAG(IS_ANDROID)
  vea_factory_functions.push_back(base::BindRepeating(&CreateAndroidVEA));
#endif
#if BUILDFLAG(IS_MAC)
  vea_factory_functions.push_back(base::BindRepeating(&CreateVTVEA));
#endif
#if BUILDFLAG(IS_WIN)
  vea_factory_functions.push_back(base::BindRepeating(
      &CreateMediaFoundationVEA, gpu_preferences, gpu_workarounds));
#endif
  return vea_factory_functions;
}

VideoEncodeAccelerator::SupportedProfiles GetSupportedProfilesInternal(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    bool populate_extended_info) {
  if (gpu_preferences.disable_accelerated_video_encode)
    return VideoEncodeAccelerator::SupportedProfiles();

  VideoEncodeAccelerator::SupportedProfiles profiles;
  for (const auto& create_vea :
       GetVEAFactoryFunctions(gpu_preferences, gpu_workarounds)) {
    auto vea = std::move(create_vea).Run();
    if (!vea)
      continue;
    auto vea_profiles = populate_extended_info
                            ? vea->GetSupportedProfiles()
                            : vea->GetSupportedProfilesLight();
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
    std::unique_ptr<MediaLog> media_log) {
  // NullMediaLog silently and safely does nothing.
  if (!media_log)
    media_log = std::make_unique<media::NullMediaLog>();

  for (const auto& create_vea :
       GetVEAFactoryFunctions(gpu_preferences, gpu_workarounds)) {
    std::unique_ptr<VideoEncodeAccelerator> vea = create_vea.Run();
    if (!vea)
      continue;
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
    bool populate_extended_info) {
  // Cache the supported profiles so that they will not be computed more than
  // once per GPU process. It is assumed that |gpu_preferences| do not change
  // between calls.
  VideoEncodeAccelerator::SupportedProfiles* profiles_ptr = nullptr;
  if (populate_extended_info) {
    static auto profiles =
        GetSupportedProfilesInternal(gpu_preferences, gpu_workarounds, true);
    profiles_ptr = &profiles;

  } else {
    static auto profiles =
        GetSupportedProfilesInternal(gpu_preferences, gpu_workarounds, false);
    profiles_ptr = &profiles;
  }

#if BUILDFLAG(USE_V4L2_CODEC)
  // V4L2-only: the encoder devices may not be visible at the time the GPU
  // process is starting. If the capabilities vector is empty, try to query the
  // devices again in the hope that they will have appeared in the meantime.
  // TODO(crbug.com/948147): trigger query when an device add/remove event
  // (e.g. via udev) has happened instead.
  if (profiles_ptr->empty()) {
    VLOGF(1) << "Supported profiles empty, querying again...";
    *profiles_ptr = GetSupportedProfilesInternal(
        gpu_preferences, gpu_workarounds, populate_extended_info);
  }
#endif

  if (gpu_workarounds.disable_accelerated_vp8_encode) {
    base::EraseIf(*profiles_ptr, [](const auto& vea_profile) {
      return vea_profile.profile == VP8PROFILE_ANY;
    });
  }

  if (gpu_workarounds.disable_accelerated_vp9_encode) {
    base::EraseIf(*profiles_ptr, [](const auto& vea_profile) {
      return vea_profile.profile >= VP9PROFILE_PROFILE0 &&
             vea_profile.profile <= VP9PROFILE_PROFILE3;
    });
  }

  if (gpu_workarounds.disable_accelerated_h264_encode) {
    base::EraseIf(*profiles_ptr, [](const auto& vea_profile) {
      return vea_profile.profile >= H264PROFILE_MIN &&
             vea_profile.profile <= H264PROFILE_MAX;
    });
  }

  base::EraseIf(*profiles_ptr, [](const auto& vea_profile) {
    return vea_profile.profile >= HEVCPROFILE_MIN &&
           vea_profile.profile <= HEVCPROFILE_MAX;
  });

  return *profiles_ptr;
}

}  // namespace media
