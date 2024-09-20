// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/codec_factory.h"

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/decoder.h"
#include "media/base/media_log.h"
#include "media/base/overlay_info.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/mojo/clients/mojo_video_encode_accelerator.h"
#include "media/video/gpu_video_accelerator_factories.h"

namespace content {

CodecFactory::CodecFactory(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    scoped_refptr<viz::ContextProviderCommandBuffer> context_provider,
    bool video_decode_accelerator_enabled,
    bool video_encode_accelerator_enabled,
    mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
        pending_vea_provider_remote)
    : media_task_runner_(std::move(media_task_runner)),
      context_provider_(std::move(context_provider)),
      video_decode_accelerator_enabled_(video_decode_accelerator_enabled),
      video_encode_accelerator_enabled_(video_encode_accelerator_enabled) {
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CodecFactory::BindOnTaskRunner, base::Unretained(this),
                     std::move(pending_vea_provider_remote)));
}
CodecFactory::~CodecFactory() = default;

std::unique_ptr<media::VideoEncodeAccelerator>
CodecFactory::CreateVideoEncodeAccelerator() {
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

  media::mojom::EncodeCommandBufferIdPtr command_buffer_id =
      media::mojom::EncodeCommandBufferId::New();
  command_buffer_id->channel_token = channel_token_;
  command_buffer_id->route_id = route_id_;
  mojo::PendingRemote<media::mojom::VideoEncodeAccelerator> vea;
  vea_provider_->CreateVideoEncodeAccelerator(
      std::move(command_buffer_id), vea.InitWithNewPipeAndPassReceiver());

  if (!vea) {
    return nullptr;
  }

  return base::WrapUnique<media::VideoEncodeAccelerator>(
      new media::MojoVideoEncodeAccelerator(std::move(vea)));
}

media::VideoDecoderType CodecFactory::GetVideoDecoderType() {
  base::AutoLock lock(supported_profiles_lock_);
  return video_decoder_type_;
}

std::optional<media::SupportedVideoDecoderConfigs>
CodecFactory::GetSupportedVideoDecoderConfigs() {
  base::AutoLock lock(supported_profiles_lock_);
  return supported_decoder_configs_;
}

std::optional<media::VideoEncodeAccelerator::SupportedProfiles>
CodecFactory::GetVideoEncodeAcceleratorSupportedProfiles() {
  base::AutoLock lock(supported_profiles_lock_);
  return supported_vea_profiles_;
}

bool CodecFactory::IsDecoderSupportKnown() {
  base::AutoLock lock(supported_profiles_lock_);
  return decoder_support_notifier_.is_notified();
}

bool CodecFactory::IsEncoderSupportKnown() {
  base::AutoLock lock(supported_profiles_lock_);
  return encoder_support_notifier_.is_notified();
}

void CodecFactory::NotifyDecoderSupportKnown(base::OnceClosure callback) {
  base::AutoLock lock(supported_profiles_lock_);
  decoder_support_notifier_.Register(
      base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void CodecFactory::NotifyEncoderSupportKnown(base::OnceClosure callback) {
  base::AutoLock lock(supported_profiles_lock_);
  encoder_support_notifier_.Register(
      base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void CodecFactory::OnChannelTokenReady(const base::UnguessableToken& token,
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

CodecFactory::Notifier::Notifier() = default;
CodecFactory::Notifier::~Notifier() = default;

void CodecFactory::Notifier::Register(base::OnceClosure callback) {
  if (is_notified_) {
    std::move(callback).Run();
    return;
  }
  callbacks_.push_back(std::move(callback));
}

void CodecFactory::Notifier::Notify() {
  DCHECK(!is_notified_);
  is_notified_ = true;
  while (!callbacks_.empty()) {
    std::move(callbacks_.back()).Run();
    callbacks_.pop_back();
  }
}

void CodecFactory::OnDecoderSupportFailed() {
  base::AutoLock lock(supported_profiles_lock_);
  if (decoder_support_notifier_.is_notified()) {
    return;
  }
  supported_decoder_configs_ = media::SupportedVideoDecoderConfigs();
  decoder_support_notifier_.Notify();
}

void CodecFactory::OnGetSupportedDecoderConfigs() {
  base::AutoLock lock(supported_profiles_lock_);
  decoder_support_notifier_.Notify();
}

void CodecFactory::OnEncoderSupportFailed() {
  base::AutoLock lock(supported_profiles_lock_);
  if (encoder_support_notifier_.is_notified()) {
    return;
  }
  supported_vea_profiles_ = media::VideoEncodeAccelerator::SupportedProfiles();
  encoder_support_notifier_.Notify();
}

void CodecFactory::OnGetVideoEncodeAcceleratorSupportedProfiles(
    const media::VideoEncodeAccelerator::SupportedProfiles&
        supported_profiles) {
  base::AutoLock lock(supported_profiles_lock_);
  supported_vea_profiles_ = supported_profiles;
  if (IsEncoderReady()) {
    encoder_support_notifier_.Notify();
  }
}

bool CodecFactory::IsEncoderReady() {
  return supported_vea_profiles_ && !channel_token_.is_empty();
}

void CodecFactory::BindOnTaskRunner(
    mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
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
    // Unretained is safe since CodecFactory is never destroyed.
    // It lives until the process shuts down.
    vea_provider_.set_disconnect_handler(base::BindOnce(
        &CodecFactory::OnEncoderSupportFailed, base::Unretained(this)));
    vea_provider_->GetVideoEncodeAcceleratorSupportedProfiles(base::BindOnce(
        &CodecFactory::OnGetVideoEncodeAcceleratorSupportedProfiles,
        base::Unretained(this)));
  } else {
    OnEncoderSupportFailed();
  }

  if (!video_decode_accelerator_enabled_) {
    OnDecoderSupportFailed();
  }
}

CodecFactoryDefault::CodecFactoryDefault(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<viz::ContextProviderCommandBuffer> context_provider,
    bool video_decode_accelerator_enabled,
    bool video_encode_accelerator_enabled,
    mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
        pending_vea_provider_remote)
    : CodecFactory(std::move(task_runner),
                   std::move(context_provider),
                   video_decode_accelerator_enabled,
                   video_encode_accelerator_enabled,
                   std::move(pending_vea_provider_remote)) {
  // There is no decoder provider.
  OnDecoderSupportFailed();
}

CodecFactoryDefault::~CodecFactoryDefault() = default;

std::unique_ptr<media::VideoDecoder> CodecFactoryDefault::CreateVideoDecoder(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    media::MediaLog* media_log,
    media::RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& rendering_color_space) {
  NOTIMPLEMENTED()
      << "CodecFactoryDefault does not have a provider to create a "
         "hardware video decoder.";
  return nullptr;
}

}  // namespace content
