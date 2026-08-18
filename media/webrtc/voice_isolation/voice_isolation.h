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

class COMPONENT_EXPORT(MEDIA_WEBRTC) VoiceIsolation {
 public:
  // Creates a VoiceIsolation object. For that it needs a pointer to the
  // `model` and correct audio params (PCM linear format). `model` needs to
  // remain valid for the lifetime of the VoiceIsolation object.
  static std::unique_ptr<VoiceIsolation> Create(
      const tflite::FlatBufferModel* model,
      const media::AudioParameters& audio_params);

  virtual ~VoiceIsolation() = default;

  VoiceIsolation(const VoiceIsolation&) = delete;
  VoiceIsolation& operator=(const VoiceIsolation&) = delete;

  // Processes audio from input_bus to output_bus. This method expects that
  // input_bus and output_bus point to different busses, have the same number of
  // channels and the same number of frames.
  virtual void ProcessAudio(const AudioBus& input_bus,
                            AudioBus& output_bus) = 0;

 protected:
  VoiceIsolation() = default;
};
}  // namespace media

#endif  // MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_H_
