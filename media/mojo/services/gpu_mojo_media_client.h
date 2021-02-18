// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_GPU_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_GPU_MOJO_MEDIA_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/android_overlay_mojo_factory.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/media_buildflags.h"
#include "media/mojo/services/mojo_media_client.h"

namespace gpu {
class GpuMemoryBufferFactory;
}  // namespace gpu

namespace media {

class MediaGpuChannelManager;

class GpuMojoMediaClient final : public MojoMediaClient {
 public:
  // |media_gpu_channel_manager| must only be used on |gpu_task_runner|, which
  // is expected to be the GPU main thread task runner.
  GpuMojoMediaClient(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      base::WeakPtr<MediaGpuChannelManager> media_gpu_channel_manager,
      gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
      AndroidOverlayMojoFactoryCB android_overlay_factory_cb);
  ~GpuMojoMediaClient() final;

  // MojoMediaClient implementation.
  SupportedVideoDecoderConfigMap GetSupportedVideoDecoderConfigs() final;
  std::unique_ptr<AudioDecoder> CreateAudioDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) final;
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      MediaLog* media_log,
      mojom::CommandBufferIdPtr command_buffer_id,
      VideoDecoderImplementation implementation,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) final;
  std::unique_ptr<CdmFactory> CreateCdmFactory(
      mojom::FrameInterfaceFactory* interface_provider) final;

 private:
  gpu::GpuPreferences gpu_preferences_;
  gpu::GpuDriverBugWorkarounds gpu_workarounds_;
  gpu::GpuFeatureInfo gpu_feature_info_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  base::WeakPtr<MediaGpuChannelManager> media_gpu_channel_manager_;
  AndroidOverlayMojoFactoryCB android_overlay_factory_cb_;
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  // Indirectly owned by GpuChildThread.
  gpu::GpuMemoryBufferFactory* const gpu_memory_buffer_factory_;
  base::Optional<SupportedVideoDecoderConfigs> cros_supported_configs_;
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#if defined(OS_WIN)
  base::Optional<SupportedVideoDecoderConfigs> d3d11_supported_configs_;
#endif  // defined(OS_WIN)

  DISALLOW_COPY_AND_ASSIGN(GpuMojoMediaClient);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_GPU_MOJO_MEDIA_CLIENT_H_
