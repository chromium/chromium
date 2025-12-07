// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_codec_factory.h"

#include <memory>
#include <optional>

#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/decoder.h"
#include "media/base/media_log.h"
#include "media/base/overlay_info.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/mojo/clients/mojo_video_encode_accelerator.h"
#include "media/video/gpu_video_accelerator_factories.h"

namespace media {

MojoCodecFactory::MojoCodecFactory(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    scoped_refptr<viz::ContextProviderCommandBuffer> context_provider,
    bool video_decode_accelerator_enabled,
    bool video_encode_accelerator_enabled,
    mojo::PendingRemote<mojom::VideoEncodeAcceleratorProvider>
        pending_vea_provider_remote)
    : media_task_runner_(std::move(media_task_runner)),
      context_provider_(std::move(context_provider)),
      video_decode_accelerator_enabled_(video_decode_accelerator_enabled),
      video_encode_accelerator_enabled_(video_encode_accelerator_enabled) {
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MojoCodecFactory::BindOnTaskRunner,
                                base::Unretained(this),
                                std::move(pending_vea_provider_remote)));
}
MojoCodecFactory::~MojoCodecFactory() = default;

std::unique_ptr<VideoEncodeAccelerator>
MojoCodecFactory::CreateVideoEncodeAccelerator() {
  DCHECK(video_encode_accelerator_enabled_);
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(vea_provider_.is_bound());
  DCHECK(!channel_token_.is_empty());

  base::AutoLock lock(supported_profiles_lock_);
  // When |supported_vea_profiles_| is empty, no hw encoder is available or
  // we have not yet gotten the supported profiles.
  if (!supported_vea_profiles_) {
    DVLOG(2) << "VEA's profiles have not yet been gotten";
  } else if (supported_vea_profiles_->empty()) {
    // There is no profile supported by VEA.
    return nullptr;
  }

  mojom::EncodeCommandBufferIdPtr command_buffer_id =
      mojom::EncodeCommandBufferId::New();
  command_buffer_id->channel_token = channel_token_;
  command_buffer_id->route_id = route_id_;
  mojo::PendingRemote<mojom::VideoEncodeAccelerator> vea;
  vea_provider_->CreateVideoEncodeAccelerator(
      std::move(command_buffer_id), vea.InitWithNewPipeAndPassReceiver());

  if (!vea) {
    return nullptr;
  }

  return base::WrapUnique<VideoEncodeAccelerator>(
      new MojoVideoEncodeAccelerator(std::move(vea)));
}

VideoDecoderType MojoCodecFactory::GetVideoDecoderType() {
  base::AutoLock lock(supported_profiles_lock_);
  return video_decoder_type_;
}

std::optional<SupportedVideoDecoderConfigs>
MojoCodecFactory::GetSupportedVideoDecoderConfigs() {
  base::AutoLock lock(supported_profiles_lock_);
  return supported_decoder_configs_;
}

std::optional<VideoEncodeAccelerator::SupportedProfiles>
MojoCodecFactory::GetVideoEncodeAcceleratorSupportedProfiles() {
  base::AutoLock lock(supported_profiles_lock_);
  return supported_vea_profiles_;
}

GpuVideoAcceleratorFactories::Supported
MojoCodecFactory::IsDecoderConfigSupported(const VideoDecoderConfig& config) {
  base::AutoLock lock(supported_profiles_lock_);
  return supported_decoder_configs_
             ? std::ranges::any_of(*supported_decoder_configs_,
                                   [&](const auto& supported) {
                                     return supported.Matches(config);
                                   })
                   ? GpuVideoAcceleratorFactories::Supported::kTrue
                   : GpuVideoAcceleratorFactories::Supported::kFalse
             : GpuVideoAcceleratorFactories::Supported::kUnknown;
}

bool MojoCodecFactory::IsDecoderSupportKnown() {
  base::AutoLock lock(supported_profiles_lock_);
  return decoder_support_notifier_.is_notified();
}

bool MojoCodecFactory::IsEncoderSupportKnown() {
  base::AutoLock lock(supported_profiles_lock_);
  return encoder_support_notifier_.is_notified();
}

void MojoCodecFactory::NotifyDecoderSupportKnown(base::OnceClosure callback) {
  base::AutoLock lock(supported_profiles_lock_);
  decoder_support_notifier_.Register(
      base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void MojoCodecFactory::NotifyEncoderSupportKnown(base::OnceClosure callback) {
  base::AutoLock lock(supported_profiles_lock_);
  encoder_support_notifier_.Register(
      base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void MojoCodecFactory::OnChannelTokenReady(const base::UnguessableToken& token,
                                           int32_t route_id) {
  base::AutoLock lock(supported_profiles_lock_);
  channel_token_ = token;
  route_id_ = route_id;
  // If encoder support failed, Notify may have already been
  // called.  Explicitly check to see if Notify has been called.
  if (IsEncoderReady() && !encoder_support_notifier_.is_notified()) {
    encoder_support_notifier_.Notify();
  }
}

MojoCodecFactory::Notifier::Notifier() = default;
MojoCodecFactory::Notifier::~Notifier() = default;

void MojoCodecFactory::Notifier::Register(base::OnceClosure callback) {
  if (is_notified_) {
    std::move(callback).Run();
    return;
  }
  callbacks_.push_back(std::move(callback));
}

void MojoCodecFactory::Notifier::Notify() {
  DCHECK(!is_notified_);
  is_notified_ = true;
  while (!callbacks_.empty()) {
    std::move(callbacks_.back()).Run();
    callbacks_.pop_back();
  }
}

void MojoCodecFactory::OnDecoderSupportFailed() {
  base::AutoLock lock(supported_profiles_lock_);
  if (decoder_support_notifier_.is_notified()) {
    return;
  }
  supported_decoder_configs_ = SupportedVideoDecoderConfigs();
  decoder_support_notifier_.Notify();
}

void MojoCodecFactory::OnGetSupportedDecoderConfigs() {
  base::AutoLock lock(supported_profiles_lock_);
  decoder_support_notifier_.Notify();
}

void MojoCodecFactory::OnEncoderSupportFailed() {
  base::AutoLock lock(supported_profiles_lock_);
  if (encoder_support_notifier_.is_notified()) {
    return;
  }
  supported_vea_profiles_ = VideoEncodeAccelerator::SupportedProfiles();
  encoder_support_notifier_.Notify();
}

void MojoCodecFactory::OnGetVideoEncodeAcceleratorSupportedProfiles(
    const VideoEncodeAccelerator::SupportedProfiles& supported_profiles) {
  base::AutoLock lock(supported_profiles_lock_);
  supported_vea_profiles_ = supported_profiles;
  if (IsEncoderReady()) {
    encoder_support_notifier_.Notify();
  }
}

bool MojoCodecFactory::IsEncoderReady() {
  return supported_vea_profiles_ && !channel_token_.is_empty();
}

void MojoCodecFactory::BindOnTaskRunner(
    mojo::PendingRemote<mojom::VideoEncodeAcceleratorProvider>
        pending_vea_provider_remote) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(context_provider_);

  vea_provider_.Bind(std::move(pending_vea_provider_remote));

  if (context_provider_->BindToCurrentSequence() !=
      gpu::ContextResult::kSuccess) {
    OnDecoderSupportFailed();
    OnEncoderSupportFailed();
    return;
  }

  if (video_encode_accelerator_enabled_) {
    // The remote might be disconnected if the encoding process crashes, for
    // example a GPU driver failure. Set a disconnect handler to watch these
    // types of failures and treat them as if there are no supported encoder
    // profiles.
    // Unretained is safe since MojoCodecFactory is never destroyed.
    // It lives until the process shuts down.
    vea_provider_.set_disconnect_handler(base::BindOnce(
        &MojoCodecFactory::OnEncoderSupportFailed, base::Unretained(this)));
    vea_provider_->GetVideoEncodeAcceleratorSupportedProfiles(base::BindOnce(
        &MojoCodecFactory::OnGetVideoEncodeAcceleratorSupportedProfiles,
        base::Unretained(this)));
  } else {
    OnEncoderSupportFailed();
  }

  if (!video_decode_accelerator_enabled_) {
    OnDecoderSupportFailed();
  }
}

MojoCodecFactoryDefault::MojoCodecFactoryDefault(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<viz::ContextProviderCommandBuffer> context_provider,
    bool video_decode_accelerator_enabled,
    bool video_encode_accelerator_enabled,
    mojo::PendingRemote<mojom::VideoEncodeAcceleratorProvider>
        pending_vea_provider_remote)
    : MojoCodecFactory(std::move(task_runner),
                       std::move(context_provider),
                       video_decode_accelerator_enabled,
                       video_encode_accelerator_enabled,
                       std::move(pending_vea_provider_remote)) {
  // There is no decoder provider.
  OnDecoderSupportFailed();
}

MojoCodecFactoryDefault::~MojoCodecFactoryDefault() = default;

std::unique_ptr<VideoDecoder> MojoCodecFactoryDefault::CreateVideoDecoder(
    GpuVideoAcceleratorFactories* gpu_factories,
    MediaLog* media_log,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& rendering_color_space) {
  NOTIMPLEMENTED()
      << "MojoCodecFactoryDefault does not have a provider to create a "
         "hardware video decoder.";
  return nullptr;
}

}  // namespace media
