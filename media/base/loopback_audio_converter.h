// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_LOOPBACK_AUDIO_CONVERTER_H_
#define MEDIA_BASE_LOOPBACK_AUDIO_CONVERTER_H_

#include "media/base/audio_converter.h"

namespace media {

// LoopbackAudioConverter works similar to AudioConverter and converts input
// streams to different audio parameters. Then, the LoopbackAudioConverter can
// be used as an input to another AudioConverter. This allows us to
// use converted audio from AudioOutputStreams as input to an AudioConverter.
// For example, this allows converting multiple streams into a common format and
// using the converted audio as input to another AudioConverter (i.e. a mixer).
class MEDIA_EXPORT LoopbackAudioConverter
    : public AudioConverter::InputCallback {
 public:
  LoopbackAudioConverter(const AudioParameters& input_params,
                         const AudioParameters& output_params,
                         bool disable_fifo);

  LoopbackAudioConverter(const LoopbackAudioConverter&) = delete;
  LoopbackAudioConverter& operator=(const LoopbackAudioConverter&) = delete;

  ~LoopbackAudioConverter() override;

  void AddInput(AudioConverter::InputCallback* input) {
    audio_converter_.AddInput(input);
  }

  void RemoveInput(AudioConverter::InputCallback* input) {
    audio_converter_.RemoveInput(input);
  }

  bool empty() { return audio_converter_.empty(); }

 private:
  double ProvideInput(AudioBus* audio_bus,
                      uint32_t frames_delayed,
                      const AudioGlitchInfo& glitch_info) override;

  AudioConverter audio_converter_;
};

}  // namespace media

#endif  // MEDIA_BASE_LOOPBACK_AUDIO_CONVERTER_H_
