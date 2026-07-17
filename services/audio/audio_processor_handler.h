// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_AUDIO_PROCESSOR_HANDLER_H_
#define SERVICES_AUDIO_AUDIO_PROCESSOR_HANDLER_H_

#include <atomic>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "media/audio/aecdump_recording_manager.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_processing.h"
#include "media/mojo/mojom/audio_processing.mojom.h"
#include "media/webrtc/audio_processor.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/audio/reference_output.h"

namespace media {
class AudioBus;
class AudioParameters;
}  // namespace media

namespace audio {
class MlModelManager;
class ProcessingAudioFifo;
class VoiceIsolationHandler;

// Encapsulates audio processing effects in the audio process, using a
// media::AudioProcessor. Forwards capture audio, playout audio, and
// control calls to the processor. If voice isolation is enabled, the
// processed audio is further routed through a VoiceIsolationHandler.
//
// The class can be operated on by three different sequences:
// - An owning sequence, which performs construction, destruction, getting
// stats, and similar control flow.
// - A capture thread, which calls ProcessCapturedAudio().
// - A playout thread, which calls OnPlayoutData().
//
// All functions should be called on the owning thread, unless otherwise
// specified. It is the responsibility of the owner to ensure that the playout
// thread and capture thread stop calling into the AudioProcessorHandler before
// destruction.
//
// Audio data flow through AudioProcessorHandler:
//
// * Without a dedicated processing thread (lightweight / no FIFO):
//   Audio capture thread:
//     AudioProcessorHandler::ProcessCapturedAudio()
//     -> AudioProcessorHandler::ProcessCapturedAudioInternal()
//     --> media::AudioProcessor (WebRTC processing)
//     ---> AudioProcessorHandler::OnAudioProcessorOutput() (callback)
//     ----> VoiceIsolationHandler::ProcessCapturedAudio() (if voice isolation
//     enabled)
//     -----> |deliver_processed_audio_callback_|
//     ----> |deliver_processed_audio_callback_| (if voice isolation disabled)
//
// * With a dedicated processing thread (heavy / using FIFO):
//   Audio capture thread:
//     AudioProcessorHandler::ProcessCapturedAudio()
//     -> |processing_fifo_|::PushData()
//   Audio processing thread:
//     -> AudioProcessorHandler::ProcessCapturedAudioInternal() (via FIFO
//     callback)
//     --> media::AudioProcessor (WebRTC processing)
//     ---> AudioProcessorHandler::OnAudioProcessorOutput() (callback)
//     ----> VoiceIsolationHandler::ProcessCapturedAudio() (if voice isolation
//     enabled)
//     -----> |deliver_processed_audio_callback_|
//     ----> |deliver_processed_audio_callback_| (if voice isolation disabled)
class AudioProcessorHandler final : public ReferenceOutput::Listener,
                                    public media::mojom::AudioProcessorControls,
                                    public media::AecdumpRecordingSource {
 public:
  static constexpr int kProcessingFifoSize = 10;

  using DeliverProcessedAudioCallback = base::RepeatingCallback<void(
      const media::AudioBus& audio_bus,
      base::TimeTicks audio_capture_time,
      std::optional<double> new_volume,
      const media::AudioGlitchInfo& audio_glitch_info)>;

  using LogCallback = base::RepeatingCallback<void(std::string_view)>;
  using ReferenceStreamErrorCallback = base::RepeatingCallback<void()>;

  // |settings| specifies which audio processing effects to apply. Some effect
  // must be required, i.e. the AudioProcessorHandler may only be created if
  // |settings.NeedAudioModification()| is true.
  // |input_format| and |output_format| specify formats before and after
  // processing, where |*_format|.frames_per_buffer() must be 10 ms if
  // |settings|.NeedWebrtcAudioProcessing().
  // |log_callback| is used for logging messages on the owning sequence.
  // |deliver_processed_audio_callback| is used to deliver processed audio
  // provided to ProcessCapturedAudio().
  // |controls_receiver| calls are received by the AudioProcessorHandler.
  // |aecdump_recording_manager| is used to register and deregister an aecdump
  // recording source, and must outlive the AudioProcessorHandler if not null.
  AudioProcessorHandler(
      const media::AudioProcessingSettings& settings,
      const media::AudioParameters& input_format,
      const media::AudioParameters& output_format,
      LogCallback log_callback,
      // Used to deliver processed audio if `voice_isolation_handler` is not
      // provided.
      DeliverProcessedAudioCallback deliver_processed_audio_callback,
      // reference_stream_error_callback will be called on the main thread.
      ReferenceStreamErrorCallback reference_stream_error_callback,
      mojo::PendingReceiver<media::mojom::AudioProcessorControls>
          controls_receiver,
      media::AecdumpRecordingManager* aecdump_recording_manager,
      raw_ptr<MlModelManager> ml_model_manager,
      std::unique_ptr<VoiceIsolationHandler> voice_isolation_handler);

  AudioProcessorHandler(const AudioProcessorHandler&) = delete;
  AudioProcessorHandler& operator=(const AudioProcessorHandler&) = delete;
  ~AudioProcessorHandler() final;

  // Prepares the handler to start processing captured audio. Must be called
  // before the capture stream is started.
  // Called on `owning_sequence_`.
  void StartProcessing();

  // Stops audio processing and resets internal resources (including destroying
  // the FIFO if present). Must be called after the capture stream has been
  // synchronously stopped. The caller must guarantee that no concurrent calls
  // to ProcessCapturedAudio() are in progress or will be made after
  // StopProcessing() starts. Called on `owning_sequence_`.
  void StopProcessing();

  // Processes and delivers captured audio.
  // See media::AudioProcessor::ProcessCapturedAudio for API details.
  // Called on the capture thread.
  void ProcessCapturedAudio(const media::AudioBus& audio_source,
                            base::TimeTicks audio_capture_time,
                            double volume,
                            const media::AudioGlitchInfo& audio_glitch_info);

  // The format of audio input to the processor; constant throughout its
  // lifetime.
  const media::AudioParameters& input_format() const {
    return audio_processor_->input_format();
  }

  // If true, `audio::ReferenceOutput::Listener::OnPlayoutData()` should be
  // called.
  bool needs_playout_reference() const {
    return audio_processor_->needs_playout_reference();
  }

 private:
  friend class InputControllerTestHelper;
  friend class AudioProcessorHandlerTest;

  // Used in the mojom::AudioProcessorControls implementation.
  using GetStatsCallback =
      base::OnceCallback<void(const media::AudioProcessingStats& stats)>;

  // ReferenceOutput::Listener implementation.
  // Called on the playout thread.
  void OnPlayoutData(const media::AudioBus& audio_bus,
                     int sample_rate,
                     base::TimeDelta delay) final;
  // Called on `owning_sequence_`.
  void OnReferenceStreamError() final;

  // mojom::AudioProcessorControls implementation.
  void GetStats(GetStatsCallback callback) final;
  void SetPreferredNumCaptureChannels(int32_t num_preferred_channels) final;

  // media::AecdumpRecordingSource implementation.
  void StartAecdump(base::File aecdump_file) final;
  void StopAecdump() final;

  void ProcessCapturedAudioInternal(
      const media::AudioBus& audio_source,
      base::TimeTicks audio_capture_time,
      double volume,
      const media::AudioGlitchInfo& audio_glitch_info);

  // Callback invoked by `audio_processor_` when it has finished WebRTC
  // processing for a frame of captured audio. Called on the capture/processing
  // thread.
  //
  // This method acts as the coordinator: it retrieves the accumulated
  // `AudioGlitchInfo` (which `AudioProcessor` doesn't track) and routes the
  // processed audio along with the glitch info to `voice_isolation_handler_`
  // if enabled, or directly to the final `deliver_processed_audio_callback_`
  // otherwise.
  void OnAudioProcessorOutput(const media::AudioBus& audio_bus,
                              base::TimeTicks audio_capture_time,
                              std::optional<double> new_volume);

  SEQUENCE_CHECKER(owning_sequence_);

  std::unique_ptr<VoiceIsolationHandler> voice_isolation_handler_;

  // The audio processor is accessed on all threads (OS capture thread, OS
  // playout thread, owning sequence) and created / destroyed on the owning
  // sequence.
  const std::unique_ptr<media::AudioProcessor> audio_processor_;

  // Used to deliver audio if it's not passed over to
  // `voice_isolation_handler_`.
  const DeliverProcessedAudioCallback deliver_processed_audio_callback_;

  const ReferenceStreamErrorCallback reference_stream_error_callback_
      GUARDED_BY_CONTEXT(owning_sequence_);

  mojo::Receiver<media::mojom::AudioProcessorControls> receiver_
      GUARDED_BY_CONTEXT(owning_sequence_);

  // Used to deregister as an aecdump recording source upon destruction.
  const raw_ptr<media::AecdumpRecordingManager> aecdump_recording_manager_
      GUARDED_BY_CONTEXT(owning_sequence_);

  // The number of channels preferred by consumers of the captured audio.
  // Initially, consumers are assumed to use mono audio in order to 1) avoid
  // unnecessary computational load and 2) preserve the historical default.
  // Written from the owning thread in SetPreferredNumCaptureChannels and read
  // from the real-time capture thread in ProcessCapturedAudio.
  // We use an atomic instead of a lock in order to avoid blocking on the
  // real-time thread.
  std::atomic<int32_t> num_preferred_channels_ = 1;

  media::AudioGlitchInfo::Accumulator glitch_info_accumulator_;

  std::unique_ptr<ProcessingAudioFifo> processing_fifo_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_AUDIO_PROCESSOR_HANDLER_H_
