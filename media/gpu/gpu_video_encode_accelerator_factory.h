// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_GPU_VIDEO_ENCODE_ACCELERATOR_FACTORY_H_
#define MEDIA_GPU_GPU_VIDEO_ENCODE_ACCELERATOR_FACTORY_H_

#include <memory>

#include "media/gpu/media_gpu_export.h"
#include "media/video/video_encode_accelerator.h"

namespace gpu {
struct GpuPreferences;
class GpuDriverBugWorkarounds;
}  // namespace gpu

namespace media {

class MEDIA_GPU_EXPORT GpuVideoEncodeAcceleratorFactory {
 public:
  // Creates and Initializes a VideoEncodeAccelerator. Returns nullptr
  // if there is no implementation available on the platform or calling
  // VideoEncodeAccelerator::Initialize() returns false.
  static std::unique_ptr<VideoEncodeAccelerator> CreateVEA(
      const VideoEncodeAccelerator::Config& config,
      VideoEncodeAccelerator::Client* client,
      const gpu::GpuPreferences& gpu_perferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds);

  // Gets the supported codec profiles for video encoding on the platform.
  static VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(GpuVideoEncodeAcceleratorFactory);
};

}  // namespace media

#endif  // MEDIA_GPU_GPU_VIDEO_ENCODE_ACCELERATOR_FACTORY_H_
