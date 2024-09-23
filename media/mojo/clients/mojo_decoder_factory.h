// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_DECODER_FACTORY_H_
#define MEDIA_MOJO_CLIENTS_MOJO_DECODER_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/decoder_factory.h"

namespace media {

namespace mojom {
class InterfaceFactory;
}

class MojoDecoderFactory final : public DecoderFactory {
 public:
  explicit MojoDecoderFactory(
      media::mojom::InterfaceFactory* interface_factory);

  MojoDecoderFactory(const MojoDecoderFactory&) = delete;
  MojoDecoderFactory& operator=(const MojoDecoderFactory&) = delete;

  ~MojoDecoderFactory() final;

  void CreateAudioDecoders(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      MediaLog* media_log,
      std::vector<std::unique_ptr<AudioDecoder>>* audio_decoders) final;

  // TODO(crbug.com/40167137): Implement GetSupportedVideoDecoderConfigs.

  void CreateVideoDecoders(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      GpuVideoAcceleratorFactories* gpu_factories,
      MediaLog* media_log,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space,
      std::vector<std::unique_ptr<VideoDecoder>>* video_decoders) final;

 private:
  raw_ptr<media::mojom::InterfaceFactory> interface_factory_;
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_DECODER_FACTORY_H_
