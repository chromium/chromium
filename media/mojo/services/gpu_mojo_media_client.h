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
#include "media/cdm/cdm_proxy.h"
#include "media/gpu/buildflags.h"
#include "media/mojo/services/mojo_media_client.h"
#include "media/video/supported_video_decoder_config.h"

namespace gpu {
class GpuMemoryBufferFactory;
}  // namespace gpu

namespace media {

class MediaGpuChannelManager;

class GpuMojoMediaClient : public MojoMediaClient {
 public:
  // |media_gpu_channel_manager| must only be used on |gpu_task_runner|, which
  // is expected to be the GPU main thread task runner.
  // |cdm_proxy_factory_cb| can be used to create a CdmProxy. May be null if
  // CdmProxy is not supported on the platform.
  GpuMojoMediaClient(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      base::WeakPtr<MediaGpuChannelManager> media_gpu_channel_manager,
      gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
      AndroidOverlayMojoFactoryCB android_overlay_factory_cb,
      CdmProxyFactoryCB cdm_proxy_factory_cb);
  ~GpuMojoMediaClient() final;

  // MojoMediaClient implementation.
  SupportedVideoDecoderConfigMap GetSupportedVideoDecoderConfigs() final;
  void Initialize(service_manager::Connector* connector) final;
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
      service_manager::mojom::InterfaceProvider* interface_provider) final;
#if BUILDFLAG(ENABLE_CDM_PROXY)
  std::unique_ptr<CdmProxy> CreateCdmProxy(const base::Token& cdm_guid) final;
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

 private:
  gpu::GpuPreferences gpu_preferences_;
  gpu::GpuDriverBugWorkarounds gpu_workarounds_;
  gpu::GpuFeatureInfo gpu_feature_info_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  base::WeakPtr<MediaGpuChannelManager> media_gpu_channel_manager_;
  AndroidOverlayMojoFactoryCB android_overlay_factory_cb_;
#if defined(OS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  // Indirectly owned by GpuChildThread.
  gpu::GpuMemoryBufferFactory* const gpu_memory_buffer_factory_;
#endif  // defined(OS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  CdmProxyFactoryCB cdm_proxy_factory_cb_;
#if defined(OS_WIN)
  base::Optional<SupportedVideoDecoderConfigs> d3d11_supported_configs_;
#endif  // defined(OS_WIN)

#if defined(OS_CHROMEOS)
  base::Optional<SupportedVideoDecoderConfigs> cros_supported_configs_;
#endif  // defined(OS_CHROMEOS)

  DISALLOW_COPY_AND_ASSIGN(GpuMojoMediaClient);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_GPU_MOJO_MEDIA_CLIENT_H_
