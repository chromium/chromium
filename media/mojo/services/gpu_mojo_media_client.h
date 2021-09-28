// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_GPU_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_GPU_MOJO_MEDIA_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/base/android_overlay_mojo_factory.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/media_buildflags.h"
#include "media/mojo/services/mojo_media_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace gpu {
class GpuMemoryBufferFactory;
}  // namespace gpu

namespace media {

class MediaGpuChannelManager;
class GpuMojoMediaClient;

using GetConfigCacheCB =
    base::RepeatingCallback<SupportedVideoDecoderConfigs()>;
using GetCommandBufferStubCB =
    base::RepeatingCallback<gpu::CommandBufferStub*()>;

// Encapsulate parameters to pass to platform-specific helpers.
struct VideoDecoderTraits {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner;
  std::unique_ptr<MediaLog> media_log;
  RequestOverlayInfoCB request_overlay_info_cb;
  const gfx::ColorSpace* const target_color_space;
  gpu::GpuMemoryBufferFactory* const gpu_memory_buffer_factory;

  // Android uses this twice.
  GetCommandBufferStubCB get_command_buffer_stub_cb;

  AndroidOverlayMojoFactoryCB android_overlay_factory_cb;

  VideoDecoderTraits(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      std::unique_ptr<MediaLog> media_log,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace* target_color_space,
      gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
      GetCommandBufferStubCB get_command_buffer_stub_cb,
      AndroidOverlayMojoFactoryCB android_overlay_factory_cb);
  ~VideoDecoderTraits();
};

class GpuMojoMediaClient final : public MojoMediaClient {
 public:
  // Implementations of platform specific media functionality can be provided
  // by overriding the appropriate methods in this interface.
  // Specification of the platform object is done by implementing the static
  // Create() method in the gpu_mojo_media_client_<platform>.cc file.
  class PlatformDelegate {
   public:
    virtual ~PlatformDelegate();

    // Instantiates the PlatformDelegate suitable for the platform.
    // Implemented in platform-specific files.
    static std::unique_ptr<PlatformDelegate> Create(GpuMojoMediaClient* client);

    // Find platform specific implementations of these in
    // gpu_mojo_media_client_{platform}.cc
    // Creates a platform-specific media::VideoDecoder.
    virtual std::unique_ptr<VideoDecoder> CreateVideoDecoder(
        const VideoDecoderTraits& traits);

    // Queries the platform-specific VideoDecoder implementation for its
    // supported profiles. Many platforms fall back to use the VDAVideoDecoder
    // so that implementation is shared, and its supported configs can be
    // queries using the |get_vda_configs| callback.
    virtual void GetSupportedVideoDecoderConfigs(
        MojoMediaClient::SupportedVideoDecoderConfigsCallback callback);

    // Creates a platform-specific media::AudioDecoder. Most platforms don't do
    // anything here, but android, for example, does.
    virtual std::unique_ptr<AudioDecoder> CreateAudioDecoder(
        scoped_refptr<base::SingleThreadTaskRunner> task_runner);

    // Creates a CDM factory, right now only used on android and chromeos.
    virtual std::unique_ptr<CdmFactory> CreateCdmFactory(
        mojom::FrameInterfaceFactory* frame_interfaces);

    // Queries the platform decoder type.
    virtual VideoDecoderType GetDecoderImplementationType();
  };

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

  GpuMojoMediaClient(const GpuMojoMediaClient&) = delete;
  GpuMojoMediaClient& operator=(const GpuMojoMediaClient&) = delete;

  // Can be used as default fallback values by platform specific
  // implementations.
  SupportedVideoDecoderConfigs GetVDAVideoDecoderConfigs();

  const gpu::GpuPreferences& gpu_preferences() { return gpu_preferences_; }

  const gpu::GpuDriverBugWorkarounds& gpu_workarounds() {
    return gpu_workarounds_;
  }

  const gpu::GpuFeatureInfo& gpu_feature_info() const {
    return gpu_feature_info_;
  }

  // MojoMediaClient implementation.
  void GetSupportedVideoDecoderConfigs(
      MojoMediaClient::SupportedVideoDecoderConfigsCallback callback) final;
  VideoDecoderType GetDecoderImplementationType() final;
  std::unique_ptr<AudioDecoder> CreateAudioDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) final;
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      MediaLog* media_log,
      mojom::CommandBufferIdPtr command_buffer_id,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) final;
  std::unique_ptr<CdmFactory> CreateCdmFactory(
      mojom::FrameInterfaceFactory* interface_provider) final;

 private:
  void OnSupportedVideoDecoderConfigs(SupportedVideoDecoderConfigs configs);

  // Cross-platform cache supported config cache.
  absl::optional<SupportedVideoDecoderConfigs> supported_config_cache_;
  std::vector<MojoMediaClient::SupportedVideoDecoderConfigsCallback>
      pending_supported_config_callbacks_;

  gpu::GpuPreferences gpu_preferences_;
  gpu::GpuDriverBugWorkarounds gpu_workarounds_;
  gpu::GpuFeatureInfo gpu_feature_info_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  base::WeakPtr<MediaGpuChannelManager> media_gpu_channel_manager_;
  AndroidOverlayMojoFactoryCB android_overlay_factory_cb_;
  gpu::GpuMemoryBufferFactory* const gpu_memory_buffer_factory_;
  std::unique_ptr<PlatformDelegate> platform_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_GPU_MOJO_MEDIA_CLIENT_H_
