// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_GPU_VIDEO_ENCODE_ACCELERATOR_FACTORY_H_
#define MEDIA_GPU_GPU_VIDEO_ENCODE_ACCELERATOR_FACTORY_H_

#include <memory>

#include "gpu/config/gpu_info.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_encode_accelerator.h"

namespace gpu {
struct GpuPreferences;
class GpuDriverBugWorkarounds;
}  // namespace gpu

namespace media {

class MEDIA_GPU_EXPORT GpuVideoEncodeAcceleratorFactory {
 public:
  GpuVideoEncodeAcceleratorFactory() = delete;
  GpuVideoEncodeAcceleratorFactory(const GpuVideoEncodeAcceleratorFactory&) =
      delete;
  GpuVideoEncodeAcceleratorFactory& operator=(
      const GpuVideoEncodeAcceleratorFactory&) = delete;

  // Creates and Initializes a VideoEncodeAccelerator. Returns nullptr
  // if there is no implementation available on the platform or calling
  // VideoEncodeAccelerator::Initialize() returns false.
  using GetCommandBufferHelperCB =
      base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()>;
  static std::unique_ptr<VideoEncodeAccelerator> CreateVEA(
      const VideoEncodeAccelerator::Config& config,
      VideoEncodeAccelerator::Client* client,
      const gpu::GpuPreferences& gpu_perferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      const gpu::GPUInfo::GPUDevice& gpu_device,
      std::unique_ptr<MediaLog> media_log = nullptr,
      GetCommandBufferHelperCB get_command_buffer_helper_cb =
          GetCommandBufferHelperCB(),
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner = nullptr);

  // Gets the supported codec profiles for video encoding on the platform.
  static VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      const gpu::GPUInfo::GPUDevice& gpu_device);
};

}  // namespace media

#endif  // MEDIA_GPU_GPU_VIDEO_ENCODE_ACCELERATOR_FACTORY_H_
