// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_decoder_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/clients/mojo_audio_decoder.h"
#include "media/mojo/clients/mojo_video_decoder.h"
#include "media/mojo/mojom/audio_decoder.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace media {

MojoDecoderFactory::MojoDecoderFactory(
    media::mojom::InterfaceFactory* interface_factory)
    : interface_factory_(interface_factory) {
  DCHECK(interface_factory_);
}

MojoDecoderFactory::~MojoDecoderFactory() = default;

void MojoDecoderFactory::CreateAudioDecoders(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaLog* media_log,
    std::vector<std::unique_ptr<AudioDecoder>>* audio_decoders) {
#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
  mojo::PendingRemote<mojom::AudioDecoder> audio_decoder;
  interface_factory_->CreateAudioDecoder(
      audio_decoder.InitWithNewPipeAndPassReceiver());

  audio_decoders->push_back(std::make_unique<MojoAudioDecoder>(
      task_runner, std::move(audio_decoder)));
#endif
}

void MojoDecoderFactory::CreateVideoDecoders(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    GpuVideoAcceleratorFactories* gpu_factories,
    MediaLog* media_log,
    const RequestOverlayInfoCB& request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space,
    std::vector<std::unique_ptr<VideoDecoder>>* video_decoders) {
#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)

#if defined(OS_WIN)
  // If the D3D11VideoDecoder is enabled, then push a kAlternate decoder ahead
  // of the default one.
  if (base::FeatureList::IsEnabled(media::kD3D11VideoDecoder)) {
    mojo::PendingRemote<mojom::VideoDecoder> d3d11_video_decoder_remote;
    interface_factory_->CreateVideoDecoder(
        d3d11_video_decoder_remote.InitWithNewPipeAndPassReceiver());

    video_decoders->push_back(std::make_unique<MojoVideoDecoder>(
        task_runner, gpu_factories, media_log,
        std::move(d3d11_video_decoder_remote),
        VideoDecoderImplementation::kAlternate, request_overlay_info_cb,
        target_color_space));
  }
#endif  // defined(OS_WIN)
  mojo::PendingRemote<mojom::VideoDecoder> d3d11_video_decoder_remote;
  interface_factory_->CreateVideoDecoder(
      d3d11_video_decoder_remote.InitWithNewPipeAndPassReceiver());

  video_decoders->push_back(std::make_unique<MojoVideoDecoder>(
      task_runner, gpu_factories, media_log,
      std::move(d3d11_video_decoder_remote),
      VideoDecoderImplementation::kDefault, request_overlay_info_cb,
      target_color_space));

#endif
}

}  // namespace media
