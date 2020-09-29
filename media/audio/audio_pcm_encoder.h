// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_PCM_ENCODER_H_
#define MEDIA_AUDIO_AUDIO_PCM_ENCODER_H_

#include "media/base/audio_encoder.h"

namespace media {

// Defines a PCM encoder, which just passes back the raw uncompressed signed
// 16-bit linear audio data.
class MEDIA_EXPORT AudioPcmEncoder : public AudioEncoder {
 public:
  AudioPcmEncoder(const AudioParameters& input_params,
                  EncodeCB encode_callback,
                  StatusCB status_callback);
  AudioPcmEncoder(const AudioPcmEncoder&) = delete;
  AudioPcmEncoder& operator=(const AudioPcmEncoder&) = delete;
  ~AudioPcmEncoder() override = default;

 protected:
  // AudioEncoder:
  void EncodeAudioImpl(const AudioBus& audio_bus,
                       base::TimeTicks capture_time) override;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_PCM_ENCODER_H_
