// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/gpu_mojo_media_client.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "build/chromeos_buildflags.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "media/base/audio_decoder.h"
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

}  // namespace

VideoDecoderTraits::~VideoDecoderTraits() = default;
VideoDecoderTraits::VideoDecoderTraits(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    std::unique_ptr<MediaLog> media_log,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace* target_color_space,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    GetCommandBufferStubCB get_command_buffer_stub_cb,
    AndroidOverlayMojoFactoryCB android_overlay_factory_cb)
    : task_runner(std::move(task_runner)),
      gpu_task_runner(std::move(gpu_task_runner)),
      media_log(std::move(media_log)),
      request_overlay_info_cb(request_overlay_info_cb),
      target_color_space(target_color_space),
      gpu_memory_buffer_factory(gpu_memory_buffer_factory),
      get_command_buffer_stub_cb(std::move(get_command_buffer_stub_cb)),
      android_overlay_factory_cb(std::move(android_overlay_factory_cb)) {}

GpuMojoMediaClient::GpuMojoMediaClient(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    base::WeakPtr<MediaGpuChannelManager> media_gpu_channel_manager,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    AndroidOverlayMojoFactoryCB android_overlay_factory_cb)
    : gpu_preferences_(gpu_preferences),
      gpu_workarounds_(gpu_workarounds),
      gpu_feature_info_(gpu_feature_info),
      gpu_task_runner_(std::move(gpu_task_runner)),
      media_gpu_channel_manager_(std::move(media_gpu_channel_manager)),
      android_overlay_factory_cb_(std::move(android_overlay_factory_cb)),
      gpu_memory_buffer_factory_(gpu_memory_buffer_factory),
      platform_(PlatformDelegate::Create(this)) {}

GpuMojoMediaClient::~GpuMojoMediaClient() = default;

GpuMojoMediaClient::PlatformDelegate::~PlatformDelegate() = default;

std::unique_ptr<VideoDecoder>
GpuMojoMediaClient::PlatformDelegate::CreateVideoDecoder(
    const VideoDecoderTraits&) {
  return nullptr;
}

void GpuMojoMediaClient::PlatformDelegate::GetSupportedVideoDecoderConfigs(
    MojoMediaClient::SupportedVideoDecoderConfigsCallback callback) {
  std::move(callback).Run({});
}

std::unique_ptr<AudioDecoder>
GpuMojoMediaClient::PlatformDelegate::CreateAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return nullptr;
}

std::unique_ptr<CdmFactory>
GpuMojoMediaClient::PlatformDelegate::CreateCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  return nullptr;
}

VideoDecoderType
GpuMojoMediaClient::PlatformDelegate::GetDecoderImplementationType() {
  return VideoDecoderType::kUnknown;
}

std::unique_ptr<AudioDecoder> GpuMojoMediaClient::CreateAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return platform_->CreateAudioDecoder(task_runner);
}

VideoDecoderType GpuMojoMediaClient::GetDecoderImplementationType() {
  return platform_->GetDecoderImplementationType();
}

void GpuMojoMediaClient::GetSupportedVideoDecoderConfigs(
    MojoMediaClient::SupportedVideoDecoderConfigsCallback callback) {
  if (supported_config_cache_) {
    DCHECK(pending_supported_config_callbacks_.empty());

    std::move(callback).Run(*supported_config_cache_);
    return;
  }

  const bool should_query = pending_supported_config_callbacks_.empty();
  pending_supported_config_callbacks_.push_back(std::move(callback));
  if (should_query) {
    // Only get configurations if there is no query already in flight.
    platform_->GetSupportedVideoDecoderConfigs(
        base::BindOnce(&GpuMojoMediaClient::OnSupportedVideoDecoderConfigs,
                       base::Unretained(this)));
  }
}

void GpuMojoMediaClient::OnSupportedVideoDecoderConfigs(
    SupportedVideoDecoderConfigs configs) {
  DCHECK(!pending_supported_config_callbacks_.empty());

  // Return the result to all pending queries.
  supported_config_cache_ = std::move(configs);
  for (auto& callback : pending_supported_config_callbacks_) {
    std::move(callback).Run(*supported_config_cache_);
  }
  pending_supported_config_callbacks_.clear();
}

SupportedVideoDecoderConfigs GpuMojoMediaClient::GetVDAVideoDecoderConfigs() {
  VideoDecodeAccelerator::Capabilities capabilities =
      GpuVideoAcceleratorUtil::ConvertGpuToMediaDecodeCapabilities(
          GpuVideoDecodeAcceleratorFactory::GetDecoderCapabilities(
              gpu_preferences_, gpu_workarounds_));
  return ConvertFromSupportedProfiles(
      capabilities.supported_profiles,
      capabilities.flags &
          VideoDecodeAccelerator::Capabilities::SUPPORTS_ENCRYPTED_STREAMS);
}

std::unique_ptr<VideoDecoder> GpuMojoMediaClient::CreateVideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaLog* media_log,
    mojom::CommandBufferIdPtr command_buffer_id,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space) {
  // All implementations require a command buffer.
  if (!command_buffer_id)
    return nullptr;
  std::unique_ptr<MediaLog> log =
      media_log ? media_log->Clone() : std::make_unique<media::NullMediaLog>();
  VideoDecoderTraits traits(
      task_runner, gpu_task_runner_, std::move(log),
      std::move(request_overlay_info_cb), &target_color_space,
      gpu_memory_buffer_factory_,
      base::BindRepeating(
          &GetCommandBufferStub, gpu_task_runner_, media_gpu_channel_manager_,
          command_buffer_id->channel_token, command_buffer_id->route_id),
      std::move(android_overlay_factory_cb_));

  return platform_->CreateVideoDecoder(traits);
}

std::unique_ptr<CdmFactory> GpuMojoMediaClient::CreateCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  return platform_->CreateCdmFactory(frame_interfaces);
}

}  // namespace media
