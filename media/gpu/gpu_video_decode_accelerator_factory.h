// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_GPU_VIDEO_DECODE_ACCELERATOR_FACTORY_H_
#define MEDIA_GPU_GPU_VIDEO_DECODE_ACCELERATOR_FACTORY_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/android_overlay_mojo_factory.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_decode_accelerator.h"

namespace gpu {
struct GpuPreferences;
}  // namespace gpu

namespace media {

class MediaLog;

class MEDIA_GPU_EXPORT GpuVideoDecodeAcceleratorFactory {
 public:
  GpuVideoDecodeAcceleratorFactory(const GpuVideoDecodeAcceleratorFactory&) =
      delete;
  GpuVideoDecodeAcceleratorFactory& operator=(
      const GpuVideoDecodeAcceleratorFactory&) = delete;

  ~GpuVideoDecodeAcceleratorFactory();

  static std::unique_ptr<GpuVideoDecodeAcceleratorFactory> Create();

  static gpu::VideoDecodeAcceleratorCapabilities GetDecoderCapabilities(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& workarounds);

  std::unique_ptr<VideoDecodeAccelerator> CreateVDA(
      VideoDecodeAccelerator::Client* client,
      const VideoDecodeAccelerator::Config& config,
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const gpu::GpuPreferences& gpu_preferences,
      MediaLog* media_log = nullptr);

 private:
  GpuVideoDecodeAcceleratorFactory();

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<VideoDecodeAccelerator> CreateD3D11VDA(
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const gpu::GpuPreferences& gpu_preferences,
      MediaLog* media_log) const;
  std::unique_ptr<VideoDecodeAccelerator> CreateDXVAVDA(
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const gpu::GpuPreferences& gpu_preferences,
      MediaLog* media_log) const;
#endif
#if BUILDFLAG(USE_VAAPI)
  std::unique_ptr<VideoDecodeAccelerator> CreateVaapiVDA(
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const gpu::GpuPreferences& gpu_preferences,
      MediaLog* media_log) const;
#elif BUILDFLAG(USE_V4L2_CODEC) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH))
  std::unique_ptr<VideoDecodeAccelerator> CreateV4L2VDA(
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const gpu::GpuPreferences& gpu_preferences,
      MediaLog* media_log) const;
  std::unique_ptr<VideoDecodeAccelerator> CreateV4L2SliceVDA(
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const gpu::GpuPreferences& gpu_preferences,
      MediaLog* media_log) const;
#endif
#if BUILDFLAG(IS_APPLE)
  std::unique_ptr<VideoDecodeAccelerator> CreateVTVDA(
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const gpu::GpuPreferences& gpu_preferences,
      MediaLog* media_log) const;
#endif
#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<VideoDecodeAccelerator> CreateAndroidVDA(
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const gpu::GpuPreferences& gpu_preferences,
      MediaLog* media_log) const;
#endif

  const AndroidOverlayMojoFactoryCB overlay_factory_cb_;
  base::ThreadChecker thread_checker_;
};

}  // namespace media

#endif  // MEDIA_GPU_GPU_VIDEO_DECODE_ACCELERATOR_FACTORY_H_
