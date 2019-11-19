// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_PCM_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_PCM_ENCODER_H_

#include <memory>

#include "base/macros.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "third_party/blink/renderer/modules/mediarecorder/audio_track_encoder.h"

namespace blink {

// A signed, 16-bit linear audio "encoder" that will just pass the audio right
// back out again.
class AudioTrackPcmEncoder : public AudioTrackEncoder {
 public:
  explicit AudioTrackPcmEncoder(OnEncodedAudioCB on_encoded_audio_cb);

  void OnSetFormat(const media::AudioParameters& params) override;
  void EncodeAudio(std::unique_ptr<media::AudioBus> input_bus,
                   base::TimeTicks capture_time) override;

 private:
  ~AudioTrackPcmEncoder() override {}

  DISALLOW_COPY_AND_ASSIGN(AudioTrackPcmEncoder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_PCM_ENCODER_H_
