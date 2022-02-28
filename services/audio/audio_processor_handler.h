// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_AUDIO_PROCESSOR_HANDLER_H_
#define SERVICES_AUDIO_AUDIO_PROCESSOR_HANDLER_H_

#include <atomic>

#include "base/sequence_checker.h"
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

// Encapsulates audio processing effects in the audio process, using a
// media::AudioProcessor. Forwards capture audio, playout audio, and
// AudioProcessorControls calls to the processor.
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
class AudioProcessorHandler final
    : public ReferenceOutput::Listener,
      public media::mojom::AudioProcessorControls {
 public:
  using DeliverProcessedAudioCallback =
      media::AudioProcessor::DeliverProcessedAudioCallback;

  using LogCallback = base::RepeatingCallback<void(base::StringPiece)>;

  // |settings| specifies which audio processing effects to apply. Some effect
  // must be required, i.e. the AudioProcessorHandler may only be created if
  // |settings.NeedAudioModification()| is true.
  // |audio_format| specifies the audio format, both before and after
  // processing. If |settings|.NeedWebrtcAudioProcessing(), then
  // |audio_format|.frames_per_buffer() must specify 10 ms.
  // TODO(https://crbug.com/1298056): Support different input vs output format
  // to avoid unnecessary resampling.
  // |log_callback| is used for logging messages on the owning sequence.
  // |deliver_processed_audio_callback| is used to deliver processed audio
  // provided to ProcessCapturedAudio().
  // |controls_receiver| calls are received by the AudioProcessorHandler.
  explicit AudioProcessorHandler(
      const media::AudioProcessingSettings& settings,
      const media::AudioParameters& audio_format,
      LogCallback log_callback,
      DeliverProcessedAudioCallback deliver_processed_audio_callback,
      mojo::PendingReceiver<media::mojom::AudioProcessorControls>
          controls_receiver);
  AudioProcessorHandler(const AudioProcessorHandler&) = delete;
  AudioProcessorHandler& operator=(const AudioProcessorHandler&) = delete;
  ~AudioProcessorHandler() final;

  // Processes and delivers capture audio.
  // See media::AudioProcessor::ProcessCapturedAudio for API details.
  // Called on the capture thread.
  void ProcessCapturedAudio(const media::AudioBus& audio_source,
                            base::TimeTicks audio_capture_time,
                            double volume,
                            bool key_pressed);

 private:
  // Used in the mojom::AudioProcessorControls implementation.
  using GetStatsCallback =
      base::OnceCallback<void(const media::AudioProcessingStats& stats)>;

  // ReferenceOutput::Listener implementation.
  // Called on the playout thread.
  void OnPlayoutData(const media::AudioBus& audio_bus,
                     int sample_rate,
                     base::TimeDelta delay) final;

  // mojom::AudioProcessorControls implementation.
  void GetStats(GetStatsCallback callback) final;
  void SetPreferredNumCaptureChannels(int32_t num_preferred_channels) final;

  SEQUENCE_CHECKER(owning_sequence_);

  // The audio processor is accessed on all threads (OS capture thread, OS
  // playout thread, owning sequence) and created / destroyed on the owning
  // sequence.
  const std::unique_ptr<media::AudioProcessor> audio_processor_;

  mojo::Receiver<media::mojom::AudioProcessorControls> receiver_
      GUARDED_BY_CONTEXT(owning_sequence_);

  // The number of channels preferred by consumers of the captured audio.
  // Initially, consumers are assumed to use mono audio in order to 1) avoid
  // unnecessary computational load and 2) preserve the historical default.
  // Written from the owning thread in SetPreferredNumCaptureChannels and read
  // from the real-time capture thread in ProcessCapturedAudio.
  // We use an atomic instead of a lock in order to avoid blocking on the
  // real-time thread.
  std::atomic<int32_t> num_preferred_channels_ = 1;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_AUDIO_PROCESSOR_HANDLER_H_
