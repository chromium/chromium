// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_DEFAULT_DECODER_FACTORY_H_
#define MEDIA_RENDERERS_DEFAULT_DECODER_FACTORY_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/decoder_factory.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT DefaultDecoderFactory final : public DecoderFactory {
 public:
  // |external_decoder_factory| is optional decoder factory that provides
  // additional decoders.
  explicit DefaultDecoderFactory(
      std::unique_ptr<DecoderFactory> external_decoder_factory);

  DefaultDecoderFactory(const DefaultDecoderFactory&) = delete;
  DefaultDecoderFactory& operator=(const DefaultDecoderFactory&) = delete;

  ~DefaultDecoderFactory() final;

  // DecoderFactory implementation.
  void CreateAudioDecoders(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      MediaLog* media_log,
      std::vector<std::unique_ptr<AudioDecoder>>* audio_decoders) final;
  void CreateVideoDecoders(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      GpuVideoAcceleratorFactories* gpu_factories,
      MediaLog* media_log,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space,
      std::vector<std::unique_ptr<VideoDecoder>>* video_decoders) final;

  // Called from the renderer thread to prevent any more decoders from being
  // vended on other threads.
  void Shutdown();

  base::WeakPtr<DecoderFactory> GetWeakPtr();

 private:
  base::Lock shutdown_lock_;
  bool is_shutdown_ GUARDED_BY(shutdown_lock_) = false;

  std::unique_ptr<DecoderFactory> external_decoder_factory_
      GUARDED_BY(shutdown_lock_);

  base::WeakPtrFactory<DefaultDecoderFactory> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_RENDERERS_DEFAULT_DECODER_FACTORY_H_
