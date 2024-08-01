// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_GPU_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_GPU_MOJO_MEDIA_CLIENT_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/base/android_overlay_mojo_factory.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/media_buildflags.h"
#include "media/mojo/services/mojo_media_client.h"

namespace media {

class MediaGpuChannelManager;

using GetConfigCacheCB =
    base::RepeatingCallback<SupportedVideoDecoderConfigs()>;
using GetCommandBufferStubCB =
    base::RepeatingCallback<gpu::CommandBufferStub*()>;

// Encapsulate parameters to pass to platform-specific helpers.
struct VideoDecoderTraits {
  scoped_refptr<base::SequencedTaskRunner> task_runner;
  std::unique_ptr<MediaLog> media_log;
  RequestOverlayInfoCB request_overlay_info_cb;
  const raw_ptr<const gfx::ColorSpace> target_color_space;

  // Windows decoders need to ensure that the cache is populated.
  GetConfigCacheCB get_cached_configs_cb;

  // Android uses this twice.
  GetCommandBufferStubCB get_command_buffer_stub_cb;

  mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder;

  VideoDecoderTraits(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<MediaLog> media_log,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace* target_color_space,
      GetConfigCacheCB get_cached_configs_cb,
      GetCommandBufferStubCB get_command_buffer_stub_cb,
      mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder);
  ~VideoDecoderTraits();
};

struct MEDIA_MOJO_EXPORT GpuMojoMediaClientTraits {
  gpu::GpuPreferences gpu_preferences;
  gpu::GpuDriverBugWorkarounds gpu_workarounds;
  gpu::GpuFeatureInfo gpu_feature_info;
  gpu::GPUInfo gpu_info;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner;

  // Only used on Android.
  AndroidOverlayMojoFactoryCB android_overlay_factory_cb;

  // |media_gpu_channel_manager| must only be used on |gpu_task_runner|, which
  // is expected to be the GPU main thread task runner.
  base::WeakPtr<MediaGpuChannelManager> media_gpu_channel_manager;

  GpuMojoMediaClientTraits(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      const gpu::GPUInfo& gpu_info,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      AndroidOverlayMojoFactoryCB android_overlay_factory_cb,
      base::WeakPtr<MediaGpuChannelManager> media_gpu_channel_manager);
  ~GpuMojoMediaClientTraits();
};

class MEDIA_MOJO_EXPORT GpuMojoMediaClient : public MojoMediaClient {
 public:
  // Creates a platform specific GpuMojoMediaClient. Must be called on
  // `traits.gpu_task_runner`.
  static std::unique_ptr<GpuMojoMediaClient> Create(
      GpuMojoMediaClientTraits& traits);
  ~GpuMojoMediaClient() override;

  GpuMojoMediaClient(const GpuMojoMediaClient&) = delete;
  GpuMojoMediaClient& operator=(const GpuMojoMediaClient&) = delete;

  const gpu::GPUInfo& gpu_info() const { return gpu_info_; }

  // MojoMediaClient implementation.
  SupportedAudioDecoderConfigs GetSupportedAudioDecoderConfigs() final;
  SupportedVideoDecoderConfigs GetSupportedVideoDecoderConfigs() final;
  VideoDecoderType GetDecoderImplementationType() final;

  std::unique_ptr<AudioDecoder> CreateAudioDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<MediaLog> media_log) final;
  std::unique_ptr<AudioEncoder> CreateAudioEncoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner) final;
#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  void NotifyDecoderSupportKnown(
      mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder,
      base::OnceCallback<void(
          mojo::PendingRemote<stable::mojom::StableVideoDecoder>)> cb) final;
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      MediaLog* media_log,
      mojom::CommandBufferIdPtr command_buffer_id,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space,
      mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder)
      final;
  std::unique_ptr<CdmFactory> CreateCdmFactory(
      mojom::FrameInterfaceFactory* interface_provider) final;

 protected:
  GpuMojoMediaClient(GpuMojoMediaClientTraits& traits);

  // Find platform specific implementations of these in
  // gpu_mojo_media_client_{platform}.cc
  // Creates a platform-specific media::VideoDecoder.
  //
  // Note: Implementations must not store a reference to |traits| because it
  // will not remain valid after this method returns.
  virtual std::unique_ptr<VideoDecoder> CreatePlatformVideoDecoder(
      VideoDecoderTraits& traits) = 0;

  // Queries the platform-specific VideoDecoder implementation for its
  // supported profiles.
  virtual std::optional<SupportedVideoDecoderConfigs>
  GetPlatformSupportedVideoDecoderConfigs() = 0;

  // Queries the platform decoder type.
  virtual VideoDecoderType GetPlatformDecoderImplementationType() = 0;

#if BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
  // Ensures that the platform video decoder supported configurations are known.
  // When they are, |cb| is called with a PendingRemote that corresponds to the
  // same connection as |oop_video_decoder| (which may be |oop_video_decoder|
  // itself). |oop_video_decoder| may be used internally to query the supported
  // configurations of an out-of-process video decoder.
  //
  // |cb| is called with |oop_video_decoder| before
  // NotifyPlatformDecoderSupport() returns if the supported configurations are
  // already known.
  //
  // This function is thread- and sequence-safe. |cb| is always called on the
  // same sequence as NotifyPlatformDecoderSupport().
  virtual void NotifyPlatformDecoderSupport(
      mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder,
      base::OnceCallback<
          void(mojo::PendingRemote<stable::mojom::StableVideoDecoder>)> cb) = 0;
#endif

  // ------------------- [ Optional implementations below ] -------------------

  // Creates a platform-specific media::AudioDecoder. Most platforms don't do
  // anything here, but android, for example, does.
  virtual std::unique_ptr<AudioDecoder> CreatePlatformAudioDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<MediaLog> media_log);

  // Creates a platform-specific media::AudioEncoder. Most platforms don't do
  // anything here.
  virtual std::unique_ptr<AudioEncoder> CreatePlatformAudioEncoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Creates a CDM factory, right now only used on android and chromeos.
  virtual std::unique_ptr<CdmFactory> CreatePlatformCdmFactory(
      mojom::FrameInterfaceFactory* frame_interfaces);

  // Queries the platform-specific AudioDecoder implementation for its
  // supported codecs.
  virtual std::optional<SupportedAudioDecoderConfigs>
  GetPlatformSupportedAudioDecoderConfigs();

  const gpu::GpuPreferences gpu_preferences_;
  const gpu::GpuDriverBugWorkarounds gpu_workarounds_;
  const gpu::GpuFeatureInfo gpu_feature_info_;
  const gpu::GPUInfo gpu_info_;
  const scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;

 private:
  // Note this should not be passed outside of this class since it must be
  // carefully used only on the GPU thread and not the media service thread.
  const base::WeakPtr<MediaGpuChannelManager> media_gpu_channel_manager_;

  // Cross-platform cache supported config cache.
  std::optional<SupportedVideoDecoderConfigs> supported_config_cache_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_GPU_MOJO_MEDIA_CLIENT_H_
