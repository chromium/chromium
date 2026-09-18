// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_VOICE_ISOLATION_HANDLER_H_
#define SERVICES_AUDIO_VOICE_ISOLATION_HANDLER_H_

#include <atomic>
#include <memory>
#include <string_view>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_parameters.h"

namespace media {
class AudioBus;
class MlModelHandle;
class VoiceIsolation;
class VoiceIsolationComponent;
}  // namespace media

namespace audio {
class MlModelManager;
class ProcessingAudioFifo;

// Encapsulates voice isolation in the audio service.
//
// Manages routing captured audio through a voice isolation pipeline. When
// created with a model handle, component initialization is dispatched
// asynchronously to base::ThreadPool so audio capture can start immediately at
// t=0 in pass-through warmup.
class VoiceIsolationHandler {
 public:
  using DeliverProcessedAudioCallback = base::RepeatingCallback<void(
      const media::AudioBus& audio_bus,
      base::TimeTicks audio_capture_time,
      const media::AudioGlitchInfo& audio_glitch_info)>;
  using LogCallback = base::RepeatingCallback<void(std::string_view)>;

  VoiceIsolationHandler(const VoiceIsolationHandler&) = delete;
  VoiceIsolationHandler& operator=(const VoiceIsolationHandler&) = delete;

  ~VoiceIsolationHandler();

  static std::unique_ptr<VoiceIsolationHandler> MaybeCreate(
      MlModelManager& ml_model_manager,
      const media::AudioParameters& output_params,
      DeliverProcessedAudioCallback deliver_processed_audio_callback,
      LogCallback log_callback = base::DoNothing());

  static std::unique_ptr<VoiceIsolationHandler> CreateForTesting(
      std::unique_ptr<media::VoiceIsolation> voice_isolation,
      const media::AudioParameters& output_params,
      DeliverProcessedAudioCallback deliver_processed_audio_callback,
      LogCallback log_callback = base::DoNothing());

  // Processes the captured audio. Called on the capture/processing thread.
  void ProcessCapturedAudio(const media::AudioBus& audio_source,
                            base::TimeTicks audio_capture_time,
                            const media::AudioGlitchInfo& audio_glitch_info);

  // Dynamic toggle for voice isolation. Called on the owning sequence.
  void SetVoiceIsolation(bool enabled);

  // Starts the processing FIFO, if present. Must be called on
  // `owning_sequence_`. The caller guarantees that no ProcessCapturedAudio()
  // calls are made before or while StartProcessing() is called. Has no effect
  // after StopProcessing().
  void StartProcessing();

  // Stops and destroys the processing FIFO, if present. Must be called on
  // `owning_sequence_`. The caller guarantees that no ProcessCapturedAudio()
  // calls are made during or after StopProcessing(). Once stopped,
  // StartProcessing() will have no effect.
  void StopProcessing();

  // Returns true if voice isolation has its own processing thread (via an
  // internal FIFO). If false, ProcessCapturedAudio() executes synchronously on
  // the caller's thread.
  bool HasProcessingThread() const;

  bool IsVoiceIsolationBypassedForTesting() const {
    return IsVoiceIsolationBypassed();
  }

  bool IsInitializedForTesting() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
    return voice_isolation_ != nullptr;
  }

  int GetFifoSizeForTesting() const;

 private:
  VoiceIsolationHandler(
      scoped_refptr<media::MlModelHandle> model_handle,
      const media::AudioParameters& output_params,
      DeliverProcessedAudioCallback deliver_processed_audio_callback,
      LogCallback log_callback);

  VoiceIsolationHandler(
      std::unique_ptr<media::VoiceIsolation> voice_isolation,
      const media::AudioParameters& output_params,
      DeliverProcessedAudioCallback deliver_processed_audio_callback,
      LogCallback log_callback);

  std::unique_ptr<ProcessingAudioFifo> MaybeCreateProcessingFifo();

  void OnComponentCreated(
      std::unique_ptr<media::VoiceIsolationComponent> component);

  bool IsVoiceIsolationBypassed() const;

  void SendLogMessage(std::string_view message);

  void ProcessCapturedAudioInternal(
      const media::AudioBus& audio_source,
      base::TimeTicks audio_capture_time,
      double volume,
      const media::AudioGlitchInfo& audio_glitch_info);

  SEQUENCE_CHECKER(owning_sequence_);

  const base::UnguessableToken id_{base::UnguessableToken::Create()};
  const scoped_refptr<media::MlModelHandle> model_handle_;
  const media::AudioParameters output_params_;
  const DeliverProcessedAudioCallback deliver_processed_audio_callback_;
  const LogCallback log_callback_;
  std::unique_ptr<media::AudioBus> output_bus_;

  // Initialized on the owning sequence and read on the real-time audio thread
  // when `bypass_voice_isolation_` is false.
  std::unique_ptr<media::VoiceIsolation> voice_isolation_;

  // Tracks whether voice isolation was requested to be enabled via
  // SetVoiceIsolation(). If false, voice isolation remains bypassed even after
  // async component creation completes.
  bool voice_isolation_enabled_ GUARDED_BY_CONTEXT(owning_sequence_) = true;

  // Whether voice isolation is currently bypassed.
  // std::atomic ensures thread-safe, lock-free toggling between the owning
  // sequence and the real-time audio thread.
  std::atomic<bool> bypass_voice_isolation_{true};

  class StartupMetricsLogger;

  // Emits metrics for async startup. Non-null only while startup is in flight.
  std::unique_ptr<StartupMetricsLogger> startup_metrics_logger_;

  // If enabled, voice isolation processing is offloaded to a dedicated
  // real-time processing thread via this FIFO.
  std::unique_ptr<ProcessingAudioFifo> processing_fifo_;

  base::WeakPtrFactory<VoiceIsolationHandler> weak_factory_{this};
};

}  // namespace audio

#endif  // SERVICES_AUDIO_VOICE_ISOLATION_HANDLER_H_
