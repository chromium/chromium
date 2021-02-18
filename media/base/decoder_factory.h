// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECODER_FACTORY_H_
#define MEDIA_BASE_DECODER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"
#include "media/base/overlay_info.h"
#include "media/base/supported_video_decoder_config.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace gfx {
class ColorSpace;
}  // namespace gfx

namespace media {

class AudioDecoder;
class GpuVideoAcceleratorFactories;
class MediaLog;
class VideoDecoder;

// A factory class for creating audio and video decoders.
class MEDIA_EXPORT DecoderFactory {
 public:
  DecoderFactory();
  virtual ~DecoderFactory();

  // Creates audio decoders and append them to the end of |audio_decoders|.
  // Decoders are single-threaded, each decoder should run on |task_runner|.
  virtual void CreateAudioDecoders(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      MediaLog* media_log,
      std::vector<std::unique_ptr<AudioDecoder>>* audio_decoders);

  // Returns the union of all decoder configs supported by the decoders created
  // when CreateVideoDecoders is called.
  // TODO(crbug.com/1173503): Rename to GetSupportedVideoDecoderConfigs after
  //                          being properly implemented for all factories.
  virtual SupportedVideoDecoderConfigs
  GetSupportedVideoDecoderConfigsForWebRTC();

  // Creates video decoders and append them to the end of |video_decoders|.
  // Decoders are single-threaded, each decoder should run on |task_runner|.
  virtual void CreateVideoDecoders(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      GpuVideoAcceleratorFactories* gpu_factories,
      MediaLog* media_log,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space,
      std::vector<std::unique_ptr<VideoDecoder>>* video_decoders);

 private:
  DISALLOW_COPY_AND_ASSIGN(DecoderFactory);
};

}  // namespace media

#endif  // MEDIA_BASE_DECODER_FACTORY_H_
