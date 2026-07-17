// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_H_
#define MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_H_

#include <memory>

#include "base/component_export.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace media {

class AudioBus;
class AudioParameters;
class ConvertingAudioFifo;
class VoiceIsolationComponent;

class COMPONENT_EXPORT(MEDIA_WEBRTC) VoiceIsolation {
 public:
  ~VoiceIsolation();

  // Processes audio from input_bus to output_bus. This method expects that
  // input_bus and output_bus point to different busses, have the same number of
  // channels and the same number of frames.
  void ProcessAudio(const AudioBus& input_bus, AudioBus& output_bus);

  // Creates a VoiceIsolation object. For that it needs a pointer to the
  // `model` and correct audio params (PCM linear format). `model` needs to
  // stay present during all the lifetime of the VoiceIsolation instance.
  static std::unique_ptr<VoiceIsolation> Create(
      const tflite::FlatBufferModel* model,
      const media::AudioParameters& audio_params);

  static std::unique_ptr<VoiceIsolation> CreateForTesting(
      const media::AudioParameters& audio_params);

 private:
  VoiceIsolation(
      std::unique_ptr<VoiceIsolationComponent> internal_voice_isolation,
      const media::AudioParameters& audio_params);
  std::unique_ptr<VoiceIsolationComponent> internal_voice_isolation_;
  std::unique_ptr<media::ConvertingAudioFifo> forward_fifo_;
  std::unique_ptr<media::ConvertingAudioFifo> backward_fifo_;
};
}  // namespace media

#endif  // MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_H_
