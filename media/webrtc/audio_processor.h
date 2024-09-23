// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_AUDIO_PROCESSOR_H_
#define MEDIA_WEBRTC_AUDIO_PROCESSOR_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_processing.h"
#include "media/base/audio_push_fifo.h"
#include "media/webrtc/audio_delay_stats_reporter.h"
#include "media/webrtc/webrtc_features.h"
#include "third_party/webrtc/api/task_queue/task_queue_base.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing_statistics.h"

namespace media {
class AudioBus;
class AudioProcessorCaptureBus;
class AudioProcessorCaptureFifo;

// This class applies audio processing effects such as echo cancellation and
// noise suppression to input capture audio (such as a microphone signal).
// Effects are applied based on configuration from AudioProcessingSettings, and
// mainly rely on an instance of the webrtc::AudioProcessing module (APM) owned
// by the AudioProcessor.
//
// The AudioProcessor can handle up to three threads (in practice, sequences):
// - An owning sequence, which performs construction, destruction, diagnostic
// recordings, and similar signals.
// - A capture thread, which calls ProcessCapturedAudio().
// - A playout thread, which calls OnPlayoutData().
//
// All member functions must be called on the owning sequence unless
// specifically documented otherwise.
//
// Thread-safe exceptions to this scheme are explicitly documented as such.
class COMPONENT_EXPORT(MEDIA_WEBRTC) AudioProcessor {
 public:
  // Callback for consuming processed capture audio.
  // |audio_bus| contains the most recent processed capture audio.
  // |new_volume| specifies a new microphone volume from the AGC. The new
  // microphone volume range is [0.0, 1.0], and is only set if the microphone
  // volume should be adjusted.
  // Called on the capture thread.
  using DeliverProcessedAudioCallback =
      base::RepeatingCallback<void(const media::AudioBus& audio_bus,
                                   base::TimeTicks audio_capture_time,
                                   std::optional<double> new_volume)>;

  using LogCallback = base::RepeatingCallback<void(std::string_view)>;

  // |deliver_processed_audio_callback| is used to deliver frames of processed
  // capture audio, from ProcessCapturedAudio(), and has to be valid for as long
  // as ProcessCapturedAudio() may be called.
  // |log_callback| is used for logging messages on the owning sequence.
  // |input_format| specifies the format of the incoming capture data.
  // |output_format| specifies the output format. If
  // |settings|.NeedWebrtcAudioProcessing() is true, then the output must be in
  // 10 ms chunks: the formats must specify |sample rate|/100 samples per buffer
  // (rounded down). Sample rates which are not divisible by 100 are supported
  // on a best-effort basis, audio quality may suffer.
  static std::unique_ptr<AudioProcessor> Create(
      DeliverProcessedAudioCallback deliver_processed_audio_callback,
      LogCallback log_callback,
      const AudioProcessingSettings& settings,
      const media::AudioParameters& input_format,
      const media::AudioParameters& output_format);

  // See Create() for details.
  AudioProcessor(
      DeliverProcessedAudioCallback deliver_processed_audio_callback,
      LogCallback log_callback,
      const media::AudioParameters& input_format,
      const media::AudioParameters& output_format,
      rtc::scoped_refptr<webrtc::AudioProcessing> webrtc_audio_processing,
      bool stereo_mirroring,
      bool needs_playout_reference);

  ~AudioProcessor();

  AudioProcessor(const AudioProcessor&) = delete;
  AudioProcessor& operator=(const AudioProcessor&) = delete;

  // Processes capture audio and delivers in chunks of <= 10 ms to
  // |deliver_processed_audio_callback_|: Each call to ProcessCapturedAudio()
  // method triggers zero or more calls to |deliver_processed_audio_callback_|,
  // depending on internal FIFO size and content. |num_preferred_channels| is
  // the highest number of channels that any sink is interested in. This can be
  // different from the number of channels in the output format. A value of -1
  // means an unknown number. If |settings|.multi_channel_capture_processing is
  // true, the number of channels of the output of the Audio Processing Module
  // (APM) will be equal to the highest observed value of num_preferred_channels
  // as long as it does not exceed the number of channels of the output format.
  // |volume| specifies the current microphone volume, in the range [0.0, 1.0].
  // Must be called on the capture thread.
  void ProcessCapturedAudio(const media::AudioBus& audio_source,
                            base::TimeTicks audio_capture_time,
                            int num_preferred_channels,
                            double volume,
                            bool key_pressed);

  // Analyzes playout audio for e.g. echo cancellation.
  // Must be called on the playout thread.
  void OnPlayoutData(const media::AudioBus& audio_bus,
                     int sample_rate,
                     base::TimeDelta audio_delay);

  // Accessor to check if WebRTC audio processing is enabled or not.
  bool has_webrtc_audio_processing() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
    return !!webrtc_audio_processing_;
  }

  // Instructs the Audio Processing Module (APM) to reduce its complexity when
  // |muted| is true. This mode is triggered when all audio tracks are disabled.
  // The default APM complexity mode is restored by |muted| set to false.
  void SetOutputWillBeMuted(bool muted);

  // Starts a new diagnostic audio recording (aecdump). If an aecdump recording
  // is already ongoing, it is stopped before starting the new one.
  void OnStartDump(base::File dump_file);

  // Stops any ongoing aecdump.
  void OnStopDump();

  // Returns any available statistics from the WebRTC audio processing module.
  // May be called on any thread.
  webrtc::AudioProcessingStats GetStats();

  std::optional<webrtc::AudioProcessing::Config>
  GetAudioProcessingModuleConfigForTesting() const {
    if (webrtc_audio_processing_) {
      return webrtc_audio_processing_->GetConfig();
    }
    return std::nullopt;
  }

  // The format of audio input to and output from the processor; constant
  // throughout AudioProcessor lifetime.
  const media::AudioParameters& input_format() const { return input_format_; }
  const media::AudioParameters& output_format() const { return output_format_; }

  // Returns an input format compatible with the specified audio processing
  // settings and device parameters. Returns nullopt if no compatible format can
  // be produced.
  static std::optional<AudioParameters> ComputeInputFormat(
      const AudioParameters& device_format,
      const AudioProcessingSettings& settings);

  // Returns an output format that minimizes delay and resampling for given
  // input format and audio processing settings.
  static AudioParameters GetDefaultOutputFormat(
      const AudioParameters& input_format,
      const AudioProcessingSettings& settings);

  // Returns true if `OnPlayoutData()` should be called.
  bool needs_playout_reference() const { return needs_playout_reference_; }

 private:
  friend class AudioProcessorTest;

  // Called by ProcessCapturedAudio().
  // Returns the new microphone volume in the range of |0.0, 1.0], or unset if
  // the volume should not be updated.
  // |num_preferred_channels| is the highest number of channels that any sink is
  // interested in. This can be different from the number of channels in the
  // output format. A value of -1 means an unknown number. If
  // |settings|.multi_channel_capture_processing is true, the number of
  // channels of the output of the Audio Processing Module (APM) will be equal
  // to the highest observed value of num_preferred_channels as long as it does
  // not exceed the number of channels of the output format.
  // Called on the capture thread.
  std::optional<double> ProcessData(const float* const* process_ptrs,
                                    int process_frames,
                                    base::TimeDelta capture_delay,
                                    double volume,
                                    bool key_pressed,
                                    int num_preferred_channels,
                                    float* const* output_ptrs);

  // Used as callback from |playout_fifo_| in OnPlayoutData().
  // Called on the playout thread.
  void AnalyzePlayoutData(const AudioBus& audio_bus, int frame_delay);

  void SendLogMessage(const std::string& message)
      VALID_CONTEXT_REQUIRED(owning_sequence_);

  SEQUENCE_CHECKER(owning_sequence_);

  // The WebRTC audio processing module (APM). Performs the bulk of the audio
  // processing and resampling algorithms.
  const rtc::scoped_refptr<webrtc::AudioProcessing> webrtc_audio_processing_;

  // If true, then the audio processor should swap the left and right channel of
  // captured stereo audio.
  const bool stereo_mirroring_;

  // If true, `OnPlayoutData()` should be called.
  const bool needs_playout_reference_;

  // Members accessed only by the owning sequence:

  // Used by SendLogMessage.
  const LogCallback log_callback_ GUARDED_BY_CONTEXT(owning_sequence_);

  // Low-priority task queue for doing AEC dump recordings. It has to
  // created/destroyed on the same sequence and it must outlive
  // any aecdump recording in |webrtc_audio_processing_|.
  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter> worker_queue_
      GUARDED_BY_CONTEXT(owning_sequence_);

  // Cached value for the playout delay latency. Updated on the playout thread
  // and read on the capture thread.
  std::atomic<base::TimeDelta> playout_delay_{base::TimeDelta()};

  // Members configured on the owning sequence in the constructor and
  // used on the capture thread:

  // FIFO to provide capture audio in chunks that can be processed by
  // webrtc::AudioProcessing.
  std::unique_ptr<AudioProcessorCaptureFifo> capture_fifo_;

  // Receives APM processing output.
  std::unique_ptr<AudioProcessorCaptureBus> output_bus_;

  // Input and output formats for capture processing.
  const media::AudioParameters input_format_;
  const media::AudioParameters output_format_;

  // Members accessed only on the capture thread:

  // Consumer of processed capture audio in ProcessCapturedAudio().
  const DeliverProcessedAudioCallback deliver_processed_audio_callback_;

  // Observed maximum number of preferred output channels. Used for not
  // performing audio processing on more channels than the sinks are interested
  // in. The value is a maximum over time and can increase but never decrease.
  // If |settings|.multi_channel_capture_processing is true, Audio Processing
  // Module (APM) will output max_num_preferred_output_channels_ channels as
  // long as it does not exceed the number of channels of the output format.
  int max_num_preferred_output_channels_ = 1;

  // For reporting audio delay stats.
  media::AudioDelayStatsReporter audio_delay_stats_reporter_;

  // Members accessed only on the playout thread:

  // FIFO to provide playout audio in chunks that can be processed by
  // webrtc::AudioProcessing.
  AudioPushFifo playout_fifo_;

  // Cached value of the playout delay before adjusting for delay introduced by
  // |playout_fifo_|.
  base::TimeDelta unbuffered_playout_delay_ = base::TimeDelta();

  // The sample rate of incoming playout audio.
  std::optional<int> playout_sample_rate_hz_ = std::nullopt;

  // Counters to avoid excessively logging errors on a real-time thread.
  size_t apm_playout_error_code_log_count_ = 0;
  size_t large_delay_log_count_ = 0;
};

}  // namespace media

#endif  // MEDIA_WEBRTC_AUDIO_PROCESSOR_H_
