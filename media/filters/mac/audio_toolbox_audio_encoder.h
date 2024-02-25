// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_MAC_AUDIO_TOOLBOX_AUDIO_ENCODER_H_
#define MEDIA_FILTERS_MAC_AUDIO_TOOLBOX_AUDIO_ENCODER_H_

#include <memory>

#include <AudioToolbox/AudioToolbox.h>

#include "media/base/audio_bus.h"
#include "media/base/audio_encoder.h"
#include "media/base/media_export.h"
#include "media/formats/mp4/aac.h"
#include "media/media_buildflags.h"

namespace media {
class AudioTimestampHelper;
class ConvertingAudioFifo;

// Audio encoder based on macOS's AudioToolbox API. The AudioToolbox
// API is required to encode codecs that aren't supported by Chromium.
class MEDIA_EXPORT AudioToolboxAudioEncoder : public AudioEncoder {
 public:
  AudioToolboxAudioEncoder();

  AudioToolboxAudioEncoder(const AudioToolboxAudioEncoder&) = delete;
  AudioToolboxAudioEncoder& operator=(const AudioToolboxAudioEncoder&) = delete;

  ~AudioToolboxAudioEncoder() override;

  // AudioEncoder implementation.
  void Initialize(const Options& options,
                  OutputCB output_cb,
                  EncoderStatusCB done_cb) override;
  void Encode(std::unique_ptr<AudioBus> audio_bus,
              base::TimeTicks capture_time,
              EncoderStatusCB done_cb) override;
  void Flush(EncoderStatusCB flush_cb) override;

 private:
  bool CreateEncoder(const AudioEncoderConfig& config,
                     const AudioStreamBasicDescription& output_format);

  void DrainFifoOutput();

  void DoEncode(const AudioBus* data);

  // "Converter" for turning raw audio into encoded samples.
  AudioConverterRef encoder_ = nullptr;

  // Actual channel count and layout from encoder, may be different than config.
  uint32_t channel_count_ = 0u;

  // Actual sample rate from the encoder, may be different than config.
  uint32_t sample_rate_ = 0u;

  EncoderStatusCB current_done_cb_;

  // Callback that delivers encoded frames.
  OutputCB output_cb_;

  // Maximum possible output size for one call to AudioConverter.
  uint32_t max_packet_size_;

  std::unique_ptr<AudioTimestampHelper> timestamp_helper_;

  std::vector<uint8_t> codec_desc_;
  std::vector<uint8_t> temp_output_buf_;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  mp4::AAC aac_config_parser_;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

  // Ensures the data sent to Encode() matches the encoder's input format.
  std::unique_ptr<ConvertingAudioFifo> fifo_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_MAC_AUDIO_TOOLBOX_AUDIO_ENCODER_H_
