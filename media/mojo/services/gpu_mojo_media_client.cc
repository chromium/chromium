// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/gpu_mojo_media_client.h"

#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_encoder.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/ipc/service/media_gpu_channel_manager.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/video/video_decode_accelerator.h"

namespace media {

namespace {

gpu::CommandBufferStub* GetCommandBufferStub(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    base::WeakPtr<MediaGpuChannelManager> media_gpu_channel_manager,
    base::UnguessableToken channel_token,
    int32_t route_id) {
  DCHECK(gpu_task_runner->BelongsToCurrentThread());
  if (!media_gpu_channel_manager)
    return nullptr;

  gpu::GpuChannel* channel =
      media_gpu_channel_manager->LookupChannel(channel_token);
  if (!channel)
    return nullptr;

  gpu::CommandBufferStub* stub = channel->LookupCommandBuffer(route_id);
  if (!stub)
    return nullptr;

#if !BUILDFLAG(IS_ANDROID)
  // Only allow stubs that have a ContextGroup, that is, the GLES2 ones. Later
  // code assumes the ContextGroup is valid. ContextGroup is used only by the
  // legacy VDA implementation, which is not supported on Android.
  if (!stub->decoder_context()->GetContextGroup()) {
    return nullptr;
  }
#endif

  return stub;
}

}  // namespace

// Forward declaration of the platform specific GpuMojoMediaClient factory
// function.
std::unique_ptr<GpuMojoMediaClient> CreateGpuMediaService(
    GpuMojoMediaClientTraits& traits);

VideoDecoderTraits::~VideoDecoderTraits() = default;
VideoDecoderTraits::VideoDecoderTraits(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<MediaLog> media_log,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace* target_color_space,
    GetConfigCacheCB get_cached_configs_cb,
    GetCommandBufferStubCB get_command_buffer_stub_cb,
    mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder)
    : task_runner(std::move(task_runner)),
      media_log(std::move(media_log)),
      request_overlay_info_cb(request_overlay_info_cb),
      target_color_space(target_color_space),
      get_cached_configs_cb(std::move(get_cached_configs_cb)),
      get_command_buffer_stub_cb(std::move(get_command_buffer_stub_cb)),
      oop_video_decoder(std::move(oop_video_decoder)) {}

GpuMojoMediaClientTraits::~GpuMojoMediaClientTraits() = default;
GpuMojoMediaClientTraits::GpuMojoMediaClientTraits(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const gpu::GPUInfo& gpu_info,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    AndroidOverlayMojoFactoryCB android_overlay_factory_cb,
    base::WeakPtr<MediaGpuChannelManager> media_gpu_channel_manager)
    : gpu_preferences(gpu_preferences),
      gpu_workarounds(gpu_workarounds),
      gpu_feature_info(gpu_feature_info),
      gpu_info(gpu_info),
      gpu_task_runner(std::move(gpu_task_runner)),
      android_overlay_factory_cb(std::move(android_overlay_factory_cb)),
      media_gpu_channel_manager(std::move(media_gpu_channel_manager)) {}

std::unique_ptr<GpuMojoMediaClient> GpuMojoMediaClient::Create(
    GpuMojoMediaClientTraits& traits) {
  DCHECK(!traits.gpu_task_runner ||
         traits.gpu_task_runner->BelongsToCurrentThread());

  auto client = CreateGpuMediaService(traits);
  DCHECK(client);

  base::UmaHistogramEnumeration("Media.GPU.VideoDecoderType",
                                client->GetDecoderImplementationType());
  return client;
}

GpuMojoMediaClient::GpuMojoMediaClient(GpuMojoMediaClientTraits& traits)
    : gpu_preferences_(std::move(traits.gpu_preferences)),
      gpu_workarounds_(std::move(traits.gpu_workarounds)),
      gpu_feature_info_(std::move(traits.gpu_feature_info)),
      gpu_info_(std::move(traits.gpu_info)),
      gpu_task_runner_(std::move(traits.gpu_task_runner)),
      media_gpu_channel_manager_(std::move(traits.media_gpu_channel_manager)) {}

GpuMojoMediaClient::~GpuMojoMediaClient() = default;

std::unique_ptr<AudioDecoder> GpuMojoMediaClient::CreateAudioDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<MediaLog> media_log) {
  return CreatePlatformAudioDecoder(std::move(task_runner),
                                    std::move(media_log));
}

std::unique_ptr<AudioEncoder> GpuMojoMediaClient::CreateAudioEncoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return base::FeatureList::IsEnabled(kPlatformAudioEncoder)
             ? CreatePlatformAudioEncoder(std::move(task_runner))
             : nullptr;
}

VideoDecoderType GpuMojoMediaClient::GetDecoderImplementationType() {
  return GetPlatformDecoderImplementationType();
}

SupportedAudioDecoderConfigs
GpuMojoMediaClient::GetSupportedAudioDecoderConfigs() {
  return GetPlatformSupportedAudioDecoderConfigs().value_or(
      SupportedAudioDecoderConfigs{});
}

SupportedVideoDecoderConfigs
GpuMojoMediaClient::GetSupportedVideoDecoderConfigs() {
  if (!supported_config_cache_) {
    // Only bother to query if accelerated video decoding is enabled.
    // (RenderMediaClient does not know about GPU features before it asks.)
    if (gpu_preferences_.disable_accelerated_video_decode ||
        (gpu_feature_info_
             .status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE] !=
         gpu::kGpuFeatureStatusEnabled)) {
      supported_config_cache_ = SupportedVideoDecoderConfigs();
    } else {
      supported_config_cache_ = GetPlatformSupportedVideoDecoderConfigs();
    }

    // Once per GPU process record accelerator information. Profile support is
    // often just manufactured and not tested, so just record the base codec.
    bool has_accelerated_h264 = false;
    bool has_accelerated_h265 = false;
    bool has_accelerated_vp9 = false;
    bool has_accelerated_av1 = false;
    if (supported_config_cache_) {
      for (const auto& config : *supported_config_cache_) {
        if (config.profile_min >= H264PROFILE_MIN &&
            config.profile_max <= H264PROFILE_MAX) {
          has_accelerated_h264 = true;
        } else if (config.profile_min >= VP9PROFILE_MIN &&
                   config.profile_max <= VP9PROFILE_MAX) {
          has_accelerated_vp9 = true;
        } else if (config.profile_min >= AV1PROFILE_MIN &&
                   config.profile_max <= AV1PROFILE_MAX) {
          has_accelerated_av1 = true;
        } else if ((config.profile_min >= HEVCPROFILE_MIN &&
                    config.profile_max <= HEVCPROFILE_MAX) ||
                   (config.profile_min >= HEVCPROFILE_EXT_MIN &&
                    config.profile_max <= HEVCPROFILE_EXT_MAX)) {
          has_accelerated_h265 = true;
        }
      }
    }

    base::UmaHistogramBoolean("Media.HasAcceleratedVideoDecode.H264",
                              has_accelerated_h264);
    base::UmaHistogramBoolean("Media.HasAcceleratedVideoDecode.H265",
                              has_accelerated_h265);
    base::UmaHistogramBoolean("Media.HasAcceleratedVideoDecode.VP9",
                              has_accelerated_vp9);
    base::UmaHistogramBoolean("Media.HasAcceleratedVideoDecode.AV1",
                              has_accelerated_av1);
  }
  return supported_config_cache_.value_or(SupportedVideoDecoderConfigs{});
}

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
void GpuMojoMediaClient::NotifyDecoderSupportKnown(
    mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder,
    base::OnceCallback<
        void(mojo::PendingRemote<stable::mojom::StableVideoDecoder>)> cb) {
#if BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
  // TODO(b/195769334): this call should ideally be guarded only by
  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER) because eventually, the GPU process
  // should not need to know what video acceleration API is used. Until then, we
  // must guard this with (USE_VAAPI || USE_V4L2_CODEC) to be able to compile
  // Linux/CrOS builds that don't use either API (e.g., linux-x64-castos).
  NotifyPlatformDecoderSupport(std::move(oop_video_decoder), std::move(cb));
#else
  DCHECK(!oop_video_decoder);
  std::move(cb).Run(std::move(oop_video_decoder));
#endif  // BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
}
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

std::unique_ptr<VideoDecoder> GpuMojoMediaClient::CreateVideoDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    MediaLog* media_log,
    mojom::CommandBufferIdPtr command_buffer_id,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space,
    mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder) {
  // Always respect GPU features.
  if (gpu_preferences_.disable_accelerated_video_decode ||
      (gpu_feature_info_
           .status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE] !=
       gpu::kGpuFeatureStatusEnabled)) {
    return nullptr;
  }
  // All implementations require a command buffer.
  if (!command_buffer_id)
    return nullptr;
  std::unique_ptr<MediaLog> log =
      media_log ? media_log->Clone() : std::make_unique<media::NullMediaLog>();
  auto get_stub_cb = base::BindRepeating(
      &GetCommandBufferStub, gpu_task_runner_, media_gpu_channel_manager_,
      command_buffer_id->channel_token, command_buffer_id->route_id);
  VideoDecoderTraits traits(
      task_runner, std::move(log), std::move(request_overlay_info_cb),
      &target_color_space,
      // CreatePlatformVideoDecoder does not keep a reference to |traits|
      // so this bound method will not outlive |this|
      base::BindRepeating(&GpuMojoMediaClient::GetSupportedVideoDecoderConfigs,
                          base::Unretained(this)),
      get_stub_cb, std::move(oop_video_decoder));

  return CreatePlatformVideoDecoder(traits);
}

std::unique_ptr<CdmFactory> GpuMojoMediaClient::CreateCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  return CreatePlatformCdmFactory(frame_interfaces);
}

std::unique_ptr<AudioDecoder> GpuMojoMediaClient::CreatePlatformAudioDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<MediaLog> media_log) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<AudioEncoder> GpuMojoMediaClient::CreatePlatformAudioEncoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<CdmFactory> GpuMojoMediaClient::CreatePlatformCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::optional<SupportedAudioDecoderConfigs>
GpuMojoMediaClient::GetPlatformSupportedAudioDecoderConfigs() {
  NOTIMPLEMENTED();
  return std::nullopt;
}

}  // namespace media
