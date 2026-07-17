// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_VOICE_ISOLATION_HANDLER_H_
#define SERVICES_AUDIO_VOICE_ISOLATION_HANDLER_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/base/audio_glitch_info.h"

namespace media {
class MlModelHandle;
}  // namespace media

namespace audio {
class MlModelManager;
}  // namespace audio

namespace media {
class AudioBus;
class AudioParameters;
class VoiceIsolation;
}  // namespace media

namespace audio {

// Encapsulates the voice isolation capability in the audio service.
//
// VoiceIsolationHandler manages routing captured audio through a voice
// isolation pipeline. Currently, it is a pass-through wrapper that forwards
// the audio and its metadata directly, serving as a placeholder for the
// voice isolation implementation.
class VoiceIsolationHandler {
 public:
  using DeliverProcessedAudioCallback = base::RepeatingCallback<void(
      const media::AudioBus& audio_bus,
      base::TimeTicks audio_capture_time,
      std::optional<double> new_volume,
      const media::AudioGlitchInfo& audio_glitch_info)>;

  VoiceIsolationHandler(const VoiceIsolationHandler&) = delete;
  VoiceIsolationHandler& operator=(const VoiceIsolationHandler&) = delete;

  ~VoiceIsolationHandler();

  static std::unique_ptr<VoiceIsolationHandler> MaybeCreate(
      MlModelManager& ml_model_manager,
      const media::AudioParameters& output_params,
      DeliverProcessedAudioCallback deliver_processed_audio_callback);

  // Processes the captured audio. Called on the capture/processing thread.
  void ProcessCapturedAudio(const media::AudioBus& audio_source,
                            base::TimeTicks audio_capture_time,
                            std::optional<double> volume,
                            const media::AudioGlitchInfo& audio_glitch_info);

 private:
  explicit VoiceIsolationHandler(
      scoped_refptr<media::MlModelHandle> model_handle,
      const media::AudioParameters& output_params,
      DeliverProcessedAudioCallback deliver_processed_audio_callback);

  const scoped_refptr<media::MlModelHandle> model_handle_;
  const std::unique_ptr<media::VoiceIsolation> voice_isolation_;
  const DeliverProcessedAudioCallback deliver_processed_audio_callback_;
  std::unique_ptr<media::AudioBus> output_bus_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_VOICE_ISOLATION_HANDLER_H_
