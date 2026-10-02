// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_H_
#define MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_H_

#include <memory>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace media {

class AudioBus;
class AudioParameters;

class COMPONENT_EXPORT(MEDIA_WEBRTC) VoiceIsolation {
 public:
  // Creates a VoiceIsolation object. For that it needs a pointer to the
  // `model` and valid 48 kHz `audio_params` (`sample_rate() == 48000`, PCM
  // linear format). `model` needs to remain valid for the lifetime of the
  // VoiceIsolation object. Returns nullptr if no component can be created from
  // `model`. Otherwise, invalid or non-48 kHz `audio_params` cause a CHECK
  // failure.
  static std::unique_ptr<VoiceIsolation> Create(
      const tflite::FlatBufferModel* model,
      const media::AudioParameters& audio_params);

  // Creates a VoiceIsolationComponent from `model`. `model` must remain valid
  // for the lifetime of the component. Returns a non-null pointer on success,
  // or a VoiceIsolationCreationResult error code on failure.
  static base::expected<std::unique_ptr<VoiceIsolationComponent>,
                        VoiceIsolationCreationResult>
  CreateComponent(const tflite::FlatBufferModel* model);

  // Creates a VoiceIsolation object wrapping an existing `component`. Requires
  // valid 48 kHz `audio_params`. `component` must process mono 20 ms frames at
  // 48 kHz, like the components returned by CreateComponent(). Both values are
  // CHECKed.
  static std::unique_ptr<VoiceIsolation> Create(
      std::unique_ptr<VoiceIsolationComponent> component,
      const media::AudioParameters& audio_params);

  virtual ~VoiceIsolation() = default;

  VoiceIsolation(const VoiceIsolation&) = delete;
  VoiceIsolation& operator=(const VoiceIsolation&) = delete;

  // Processes audio from input_bus to output_bus. This method expects that
  // input_bus and output_bus point to different busses, have the same number of
  // channels and the same number of frames.
  virtual void ProcessAudio(const AudioBus& input_bus,
                            AudioBus& output_bus) = 0;

  // Clears all internal state and buffers. In multi-threaded environments it
  // should be called from the same sequence as `ProcessAudio`.
  virtual void ClearBuffers() = 0;

 protected:
  VoiceIsolation() = default;
};
}  // namespace media

#endif  // MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_H_
