// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/gpu_mojo_media_client.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_encoder.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/video_decoder.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/gpu_video_decode_accelerator_factory.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/ipc/service/media_gpu_channel_manager.h"
#include "media/gpu/ipc/service/vda_video_decoder.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/video/video_decode_accelerator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  // Only allow stubs that have a ContextGroup, that is, the GLES2 ones. Later
  // code assumes the ContextGroup is valid.
  if (!stub->decoder_context()->GetContextGroup())
    return nullptr;

  return stub;
}

SupportedVideoDecoderConfigs GetVDAVideoDecoderConfigs(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds) {
  VideoDecodeAccelerator::Capabilities capabilities =
      GpuVideoAcceleratorUtil::ConvertGpuToMediaDecodeCapabilities(
          GpuVideoDecodeAcceleratorFactory::GetDecoderCapabilities(
              gpu_preferences, gpu_workarounds));
  return ConvertFromSupportedProfiles(
      capabilities.supported_profiles,
      capabilities.flags &
          VideoDecodeAccelerator::Capabilities::SUPPORTS_ENCRYPTED_STREAMS);
}

}  // namespace

VideoDecoderTraits::~VideoDecoderTraits() = default;
VideoDecoderTraits::VideoDecoderTraits(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    std::unique_ptr<MediaLog> media_log,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace* target_color_space,
    gpu::GpuPreferences gpu_preferences,
    gpu::GpuFeatureInfo gpu_feature_info,
    gpu::GPUInfo gpu_info,
    const gpu::GpuDriverBugWorkarounds* gpu_workarounds,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    GetConfigCacheCB get_cached_configs_cb,
    GetCommandBufferStubCB get_command_buffer_stub_cb,
    AndroidOverlayMojoFactoryCB android_overlay_factory_cb,
    mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder)
    : task_runner(std::move(task_runner)),
      gpu_task_runner(std::move(gpu_task_runner)),
      media_log(std::move(media_log)),
      request_overlay_info_cb(request_overlay_info_cb),
      target_color_space(target_color_space),
      gpu_preferences(gpu_preferences),
      gpu_feature_info(gpu_feature_info),
      gpu_info(gpu_info),
      gpu_workarounds(gpu_workarounds),
      gpu_memory_buffer_factory(gpu_memory_buffer_factory),
      get_cached_configs_cb(std::move(get_cached_configs_cb)),
      get_command_buffer_stub_cb(std::move(get_command_buffer_stub_cb)),
      android_overlay_factory_cb(std::move(android_overlay_factory_cb)),
      oop_video_decoder(std::move(oop_video_decoder)) {}

GpuMojoMediaClient::GpuMojoMediaClient(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const gpu::GPUInfo& gpu_info,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    base::WeakPtr<MediaGpuChannelManager> media_gpu_channel_manager,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    AndroidOverlayMojoFactoryCB android_overlay_factory_cb)
    : gpu_preferences_(gpu_preferences),
      gpu_workarounds_(gpu_workarounds),
      gpu_feature_info_(gpu_feature_info),
      gpu_info_(gpu_info),
      gpu_task_runner_(std::move(gpu_task_runner)),
      media_gpu_channel_manager_(std::move(media_gpu_channel_manager)),
      android_overlay_factory_cb_(std::move(android_overlay_factory_cb)),
      gpu_memory_buffer_factory_(gpu_memory_buffer_factory) {
  base::UmaHistogramEnumeration("Media.GPU.VideoDecoderType",
                                GetDecoderImplementationType());
}

GpuMojoMediaClient::~GpuMojoMediaClient() = default;

std::unique_ptr<AudioDecoder> GpuMojoMediaClient::CreateAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return CreatePlatformAudioDecoder(std::move(task_runner));
}

std::unique_ptr<AudioEncoder> GpuMojoMediaClient::CreateAudioEncoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return base::FeatureList::IsEnabled(kPlatformAudioEncoder)
             ? CreatePlatformAudioEncoder(std::move(task_runner))
             : nullptr;
}

VideoDecoderType GpuMojoMediaClient::GetDecoderImplementationType() {
  return GetPlatformDecoderImplementationType(gpu_workarounds_,
                                              gpu_preferences_, gpu_info_);
}

SupportedVideoDecoderConfigs
GpuMojoMediaClient::GetSupportedVideoDecoderConfigs() {
  if (!supported_config_cache_) {
    supported_config_cache_ = GetSupportedVideoDecoderConfigsStatic(
        gpu_preferences_, gpu_workarounds_, gpu_info_);

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

absl::optional<SupportedVideoDecoderConfigs>
GpuMojoMediaClient::GetSupportedVideoDecoderConfigsStatic(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    const gpu::GPUInfo& gpu_info) {
  return GetPlatformSupportedVideoDecoderConfigs(
      gpu_workarounds, gpu_preferences, gpu_info,
      base::BindOnce(&GetVDAVideoDecoderConfigs, gpu_preferences,
                     gpu_workarounds));
}

std::unique_ptr<VideoDecoder> GpuMojoMediaClient::CreateVideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaLog* media_log,
    mojom::CommandBufferIdPtr command_buffer_id,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space,
    mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder) {
  // All implementations require a command buffer.
  if (!command_buffer_id)
    return nullptr;
  std::unique_ptr<MediaLog> log =
      media_log ? media_log->Clone() : std::make_unique<media::NullMediaLog>();
  VideoDecoderTraits traits(
      task_runner, gpu_task_runner_, std::move(log),
      std::move(request_overlay_info_cb), &target_color_space, gpu_preferences_,
      gpu_feature_info_, gpu_info_, &gpu_workarounds_,
      gpu_memory_buffer_factory_,
      // CreatePlatformVideoDecoder does not keep a reference to |traits|
      // so this bound method will not outlive |this|
      base::BindRepeating(&GpuMojoMediaClient::GetSupportedVideoDecoderConfigs,
                          base::Unretained(this)),
      base::BindRepeating(
          &GetCommandBufferStub, gpu_task_runner_, media_gpu_channel_manager_,
          command_buffer_id->channel_token, command_buffer_id->route_id),
      android_overlay_factory_cb_, std::move(oop_video_decoder));

  return CreatePlatformVideoDecoder(traits);
}

std::unique_ptr<CdmFactory> GpuMojoMediaClient::CreateCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  return CreatePlatformCdmFactory(frame_interfaces);
}

}  // namespace media
