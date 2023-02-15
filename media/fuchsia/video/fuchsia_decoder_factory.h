// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_VIDEO_FUCHSIA_DECODER_FACTORY_H_
#define MEDIA_FUCHSIA_VIDEO_FUCHSIA_DECODER_FACTORY_H_

#include "base/task/sequenced_task_runner.h"
#include "media/base/decoder_factory.h"
#include "media/mojo/mojom/fuchsia_media.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace media {

class FuchsiaDecoderFactory final : public DecoderFactory {
 public:
  FuchsiaDecoderFactory(
      mojo::PendingRemote<media::mojom::FuchsiaMediaCodecProvider>
          resource_provider,
      bool allow_overlays);
  ~FuchsiaDecoderFactory() override;

  // DecoderFactory implementation.
  void CreateAudioDecoders(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      MediaLog* media_log,
      std::vector<std::unique_ptr<AudioDecoder>>* audio_decoders) override;
  void CreateVideoDecoders(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      GpuVideoAcceleratorFactories* gpu_factories,
      MediaLog* media_log,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space,
      std::vector<std::unique_ptr<VideoDecoder>>* video_decoders) override;

 private:
  const mojo::SharedRemote<media::mojom::FuchsiaMediaCodecProvider>
      resource_provider_;
  const bool allow_overlays_;
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_VIDEO_FUCHSIA_DECODER_FACTORY_H_
