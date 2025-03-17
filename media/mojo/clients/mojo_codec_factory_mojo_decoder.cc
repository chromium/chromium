// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_codec_factory_mojo_decoder.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/overlay_info.h"
#include "media/mojo/clients/mojo_video_decoder.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace media {

MojoCodecFactoryMojoDecoder::MojoCodecFactoryMojoDecoder(
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    scoped_refptr<viz::ContextProviderCommandBuffer> context_provider,
    bool video_decode_accelerator_enabled,
    bool video_encode_accelerator_enabled,
    mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
        pending_vea_provider_remote,
    mojo::PendingRemote<media::mojom::InterfaceFactory>
        pending_interface_factory_remote)
    : MojoCodecFactory(std::move(media_task_runner),
                       std::move(context_provider),
                       video_decode_accelerator_enabled,
                       video_encode_accelerator_enabled,
                       std::move(pending_vea_provider_remote)) {
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MojoCodecFactoryMojoDecoder::BindOnTaskRunner,
                                base::Unretained(this),
                                std::move(pending_interface_factory_remote)));
}
MojoCodecFactoryMojoDecoder::~MojoCodecFactoryMojoDecoder() = default;

std::unique_ptr<media::VideoDecoder>
MojoCodecFactoryMojoDecoder::CreateVideoDecoder(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    media::MediaLog* media_log,
    media::RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& rendering_color_space) {
  DCHECK(video_decode_accelerator_enabled_);
  DCHECK(interface_factory_.is_bound());

  mojo::PendingRemote<media::mojom::VideoDecoder> video_decoder;
  interface_factory_->CreateVideoDecoder(
      video_decoder.InitWithNewPipeAndPassReceiver(), /*dst_video_decoder=*/{});
  return std::make_unique<media::MojoVideoDecoder>(
      media_task_runner_, gpu_factories, media_log, std::move(video_decoder),
      std::move(request_overlay_info_cb), rendering_color_space);
}

void MojoCodecFactoryMojoDecoder::BindOnTaskRunner(
    mojo::PendingRemote<media::mojom::InterfaceFactory>
        interface_factory_remote) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(context_provider_);

  interface_factory_.Bind(std::move(interface_factory_remote));

  if (!video_decode_accelerator_enabled_) {
    OnDecoderSupportFailed();
    return;
  }

  // Note: This is a bit of a hack, since we don't specify the implementation
  // before asking for the map of supported configs.  We do this because it
  // (a) saves an ipc call, and (b) makes the return of those configs atomic.
  //
  // The remote might be disconnected if the decoding process crashes, for
  // example a GPU driver failure. Set a disconnect handler to watch these
  // types of failures and treat them as if there are no supported decoder
  // configs.
  // Unretained is safe since MojoCodecFactory is never destroyed.
  // It lives until the process shuts down.

  mojo::Remote<media::mojom::VideoDecoder> video_decoder;
  interface_factory_->CreateVideoDecoder(
      video_decoder.BindNewPipeAndPassReceiver(), /*dst_video_decoder=*/{});
  video_decoder.set_disconnect_handler(
      base::BindOnce(&MojoCodecFactoryMojoDecoder::OnDecoderSupportFailed,
                     base::Unretained(this)));
  video_decoder->GetSupportedConfigs(
      base::BindOnce(&MojoCodecFactoryMojoDecoder::OnGetSupportedDecoderConfigs,
                     base::Unretained(this)));
  video_decoder_ = std::move(video_decoder);
}

void MojoCodecFactoryMojoDecoder::OnGetSupportedDecoderConfigs(
    const media::SupportedVideoDecoderConfigs& supported_configs,
    media::VideoDecoderType decoder_type) {
  {
    base::AutoLock lock(supported_profiles_lock_);
    video_decoder_.reset();
    supported_decoder_configs_.emplace(supported_configs);
    video_decoder_type_ = decoder_type;
  }
  MojoCodecFactory::OnGetSupportedDecoderConfigs();
}

}  // namespace media
