// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_decoder_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/clients/mojo_audio_decoder.h"
#include "media/mojo/clients/mojo_video_decoder.h"
#include "media/mojo/mojom/audio_decoder.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
#include "media/mojo/clients/mojo_stable_video_decoder.h"
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

namespace media {

namespace {

#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)

void CreateMojoVideoDecoder(
    media::mojom::InterfaceFactory* interface_factory,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    GpuVideoAcceleratorFactories* gpu_factories,
    MediaLog* media_log,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space,
    std::vector<std::unique_ptr<VideoDecoder>>* video_decoders) {
  mojo::PendingRemote<mojom::VideoDecoder> video_decoder_remote;
  interface_factory->CreateVideoDecoder(
      video_decoder_remote.InitWithNewPipeAndPassReceiver(),
      /*dst_video_decoder=*/{});

  video_decoders->push_back(std::make_unique<MojoVideoDecoder>(
      task_runner, gpu_factories, media_log, std::move(video_decoder_remote),
      std::move(request_overlay_info_cb), target_color_space));
}

#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)

}  // namespace

MojoDecoderFactory::MojoDecoderFactory(
    media::mojom::InterfaceFactory* interface_factory)
    : interface_factory_(interface_factory) {
  DCHECK(interface_factory_);
}

MojoDecoderFactory::~MojoDecoderFactory() = default;

void MojoDecoderFactory::CreateAudioDecoders(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    MediaLog* media_log,
    std::vector<std::unique_ptr<AudioDecoder>>* audio_decoders) {
#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
  mojo::PendingRemote<mojom::AudioDecoder> audio_decoder;
  interface_factory_->CreateAudioDecoder(
      audio_decoder.InitWithNewPipeAndPassReceiver());

  audio_decoders->push_back(std::make_unique<MojoAudioDecoder>(
      task_runner, media_log, std::move(audio_decoder)));
#endif
}

void MojoDecoderFactory::CreateVideoDecoders(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    GpuVideoAcceleratorFactories* gpu_factories,
    MediaLog* media_log,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space,
    std::vector<std::unique_ptr<VideoDecoder>>* video_decoders) {
#if BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  switch (GetOutOfProcessVideoDecodingMode()) {
    case OOPVDMode::kEnabledWithoutGpuProcessAsProxy: {
      mojo::PendingRemote<stable::mojom::StableVideoDecoder>
          stable_video_decoder_remote;
      interface_factory_->CreateStableVideoDecoder(
          stable_video_decoder_remote.InitWithNewPipeAndPassReceiver());

      video_decoders->push_back(std::make_unique<MojoStableVideoDecoder>(
          task_runner, gpu_factories, media_log,
          std::move(stable_video_decoder_remote)));
      break;
    }
    case OOPVDMode::kEnabledWithGpuProcessAsProxy:
    case OOPVDMode::kDisabled:
      CreateMojoVideoDecoder(interface_factory_, std::move(task_runner),
                             gpu_factories, media_log,
                             std::move(request_overlay_info_cb),
                             target_color_space, video_decoders);
      break;
  }
#else
  CreateMojoVideoDecoder(
      interface_factory_, std::move(task_runner), gpu_factories, media_log,
      std::move(request_overlay_info_cb), target_color_space, video_decoders);
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

#endif  // BUILDFLAG(ENABLE_MOJO_VIDEO_DECODER)
}

}  // namespace media
