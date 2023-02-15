// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/codec_factory_fuchsia.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "content/renderer/media/codec_factory.h"
#include "media/base/decoder.h"
#include "media/base/overlay_info.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_decoder.h"
#include "media/fuchsia/video/fuchsia_video_decoder.h"
#include "media/mojo/mojom/fuchsia_media.mojom.h"
#include "media/video/gpu_video_accelerator_factories.h"

namespace content {

CodecFactoryFuchsia::CodecFactoryFuchsia(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    scoped_refptr<viz::ContextProviderCommandBuffer> context_provider,
    bool video_decode_accelerator_enabled,
    bool video_encode_accelerator_enabled,
    mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
        pending_vea_provider_remote,
    mojo::PendingRemote<media::mojom::FuchsiaMediaCodecProvider>
        pending_media_codec_provider_remote)
    : CodecFactory(std::move(media_task_runner),
                   std::move(context_provider),
                   video_decode_accelerator_enabled,
                   video_encode_accelerator_enabled,
                   std::move(pending_vea_provider_remote)) {
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CodecFactoryFuchsia::BindOnTaskRunner,
                     base::Unretained(this),
                     std::move(pending_media_codec_provider_remote)));
}
CodecFactoryFuchsia::~CodecFactoryFuchsia() = default;

std::unique_ptr<media::VideoDecoder> CodecFactoryFuchsia::CreateVideoDecoder(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    media::MediaLog* media_log,
    media::RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& rendering_color_space) {
  DCHECK(video_decode_accelerator_enabled_);

  return std::make_unique<media::FuchsiaVideoDecoder>(context_provider_,
                                                      media_codec_provider_,
                                                      /*allow_overlays=*/true);
}

void CodecFactoryFuchsia::BindOnTaskRunner(
    mojo::PendingRemote<media::mojom::FuchsiaMediaCodecProvider>
        media_codec_provider_remote) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (!video_decode_accelerator_enabled_) {
    OnDecoderSupportFailed();
    return;
  }

  media_codec_provider_.Bind(std::move(media_codec_provider_remote),
                             media_task_runner_);
  // The remote might be disconnected if the decoding process crashes, for
  // example a decoder driver failure. Set a disconnect handler to watch these
  // types of failures and treat them as if there are no supported decoder
  // configs.
  // Unretained is safe since CodecFactory is never destroyed.
  // It lives until the process shuts down.
  media_codec_provider_.set_disconnect_handler(
      base::BindOnce(&CodecFactoryFuchsia::OnDecoderSupportFailed,
                     base::Unretained(this)),
      media_task_runner_);
  media_codec_provider_->GetSupportedVideoDecoderConfigs(
      base::BindOnce(&CodecFactoryFuchsia::OnGetSupportedDecoderConfigs,
                     base::Unretained(this)));
}

void CodecFactoryFuchsia::OnGetSupportedDecoderConfigs(
    const media::SupportedVideoDecoderConfigs& supported_configs) {
  {
    base::AutoLock lock(supported_profiles_lock_);
    supported_decoder_configs_.emplace(supported_configs);
    video_decoder_type_ = media::VideoDecoderType::kFuchsia;
  }
  CodecFactory::OnGetSupportedDecoderConfigs();
}

}  // namespace content
