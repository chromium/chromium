// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSOR_H_

#include <memory>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/base/audio_parameters.h"
#include "media/webrtc/audio_delay_stats_reporter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/mediastream/aec_dump_agent_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_source.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"
#include "third_party/webrtc/rtc_base/task_queue.h"

namespace media {
class AudioBus;
}  // namespace media

namespace blink {

class AecDumpAgentImpl;
class MediaStreamAudioBus;
class MediaStreamAudioFifo;
class WebRtcAudioDeviceImpl;

using webrtc::AudioProcessorInterface;

// This class owns an object of webrtc::AudioProcessing which contains signal
// processing components like AGC, AEC and NS. It enables the components based
// on the getUserMedia constraints, processes the data and outputs it in a unit
// of 10 ms data chunk.
class MODULES_EXPORT MediaStreamAudioProcessor
    : public WebRtcPlayoutDataSource::Sink,
      public AudioProcessorInterface,
      public AecDumpAgentImpl::Delegate {
 public:
  // |playout_data_source| is used to register this class as a sink to the
  // WebRtc playout data for processing AEC. If clients do not enable AEC,
  // |playout_data_source| won't be used.
  //
  // Threading note: The constructor assumes it is being run on the main render
  // thread.
  MediaStreamAudioProcessor(
      const AudioProcessingProperties& properties,
      bool use_capture_multi_channel_processing,
      scoped_refptr<WebRtcAudioDeviceImpl> playout_data_source);

  // Called when the format of the capture data has changed.
  // Called on the main render thread. The caller is responsible for stopping
  // the capture thread before calling this method.
  // After this method, the capture thread will be changed to a new capture
  // thread.
  void OnCaptureFormatChanged(const media::AudioParameters& source_params);

  // Pushes capture data in |audio_source| to the internal FIFO. Each call to
  // this method should be followed by calls to ProcessAndConsumeData() while
  // it returns false, to pull out all available data.
  // Called on the capture audio thread.
  void PushCaptureData(const media::AudioBus& audio_source,
                       base::TimeDelta capture_delay);

  // Processes a block of 10 ms data from the internal FIFO, returning true if
  // |processed_data| contains the result. Returns false and does not modify the
  // outputs if the internal FIFO has insufficient data. The caller does NOT own
  // the object pointed to by |*processed_data|.
  // |num_preferred_channels| is the highest number of channels that any sink is
  // interested in. This can be different from the number of channels in the
  // output format. A value of -1 means an unknown number. If
  // use_capture_multi_channel_processing_ is true, the number of channels of
  // the output of the Audio Processing Module (APM) will be equal to the
  // highest observed value of num_preferred_channels as long as it does not
  // exceed the number of channels of the output format.
  // |capture_delay| is an adjustment on the |capture_delay| value provided in
  // the last call to PushCaptureData().
  // |new_volume| receives the new microphone volume from the AGC. The new
  // microphone volume range is [0, 255], and the value will be 0 if the
  // microphone volume should not be adjusted.
  // Called on the capture audio thread.
  bool ProcessAndConsumeData(int volume,
                             int num_preferred_channels,
                             bool key_pressed,
                             media::AudioBus** processed_data,
                             base::TimeDelta* capture_delay,
                             int* new_volume);

  // Stops the audio processor, no more AEC dump or render data after calling
  // this method.
  void Stop();

  // The audio formats of the capture input to and output from the processor.
  // Must only be called on the main render or audio capture threads.
  const media::AudioParameters& InputFormat() const;
  const media::AudioParameters& OutputFormat() const;

  // Accessor to check if the audio processing is enabled or not.
  bool has_audio_processing() const { return !!audio_processing_; }

  // Instructs the Audio Processing Module (APM) to reduce its complexity when
  // |muted| is true. This mode is triggered when all audio tracks are disabled.
  // The default APM complexity mode is restored by |muted| set to false.
  void SetOutputWillBeMuted(bool muted);

  // AecDumpAgentImpl::Delegate implementation.
  // Called on the main render thread.
  void OnStartDump(base::File dump_file) override;
  void OnStopDump() override;

  // Returns true if MediaStreamAudioProcessor would modify the audio signal,
  // based on |properties|. If the audio signal would not be modified, there is
  // no need to instantiate a MediaStreamAudioProcessor and feed audio through
  // it. Doing so would waste a non-trivial amount of memory and CPU resources.
  static bool WouldModifyAudio(const AudioProcessingProperties& properties);

 protected:
  ~MediaStreamAudioProcessor() override;

 private:
  friend class MediaStreamAudioProcessorTest;

  FRIEND_TEST_ALL_PREFIXES(MediaStreamAudioProcessorTest,
                           TestAgcEnableDefaultAgc1);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamAudioProcessorTest,
                           TestAgcEnableExperimentalAgc1);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamAudioProcessorTest,
                           TestAgcEnableHybridAgc);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamAudioProcessorTest,
                           TestAgcEnableHybridAgcDryRun);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamAudioProcessorTest,
                           TestAgcEnableHybridAgcSimdNotAllowed);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamAudioProcessorTest,
                           TestAgcEnableClippingControl);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamAudioProcessorTest,
                           TestAgcEnableClippingControlDefaultParams);

  // WebRtcPlayoutDataSource::Sink implementation.
  void OnPlayoutData(media::AudioBus* audio_bus,
                     int sample_rate,
                     base::TimeDelta audio_delay) override;
  void OnPlayoutDataSourceChanged() override;
  void OnRenderThreadChanged() override;

  absl::optional<webrtc::AudioProcessing::Config>
  GetAudioProcessingModuleConfig() const {
    if (audio_processing_) {
      return audio_processing_->GetConfig();
    }
    return absl::nullopt;
  }

  // This method is called on the libjingle thread.
  AudioProcessorStatistics GetStats(bool has_remote_tracks) override;

  // Helper to initialize the WebRtc AudioProcessing.
  void InitializeAudioProcessingModule(
      const AudioProcessingProperties& properties);

  // Helper to initialize the capture converter.
  void InitializeCaptureFifo(const media::AudioParameters& input_format);

  // Called by ProcessAndConsumeData().
  // Returns the new microphone volume in the range of |0, 255].
  // When the volume does not need to be updated, it returns 0.
  // |num_preferred_channels| is the highest number of channels that any sink is
  // interested in. This can be different from the number of channels in the
  // output format. A value of -1 means an unknown number. If
  // use_capture_multi_channel_processing_ is true, the number of channels of
  // the output of the Audio Processing Module (APM) will be equal to the
  // highest observed value of num_preferred_channels as long as it does not
  // exceed the number of channels of the output format.
  int ProcessData(const float* const* process_ptrs,
                  int process_frames,
                  base::TimeDelta capture_delay,
                  int volume,
                  bool key_pressed,
                  int num_preferred_channels,
                  float* const* output_ptrs);

  // Update AEC stats. Called on the main render thread.
  void UpdateAecStats();

  void SendLogMessage(const WTF::String& message);

  // Cached value for the render delay latency. This member is accessed by
  // both the capture audio thread and the render audio thread.
  std::atomic<base::TimeDelta> render_delay_;

  // For reporting audio delay stats.
  media::AudioDelayStatsReporter audio_delay_stats_reporter_;

  // Low-priority task queue for doing AEC dump recordings. It has to
  // out-live audio_processing_ and be created/destroyed from the same
  // thread.
  std::unique_ptr<rtc::TaskQueue> worker_queue_;

  // Module to handle processing and format conversion.
  rtc::scoped_refptr<webrtc::AudioProcessing> audio_processing_;

  // FIFO to provide 10 ms capture chunks.
  std::unique_ptr<MediaStreamAudioFifo> capture_fifo_;
  // Receives processing output.
  std::unique_ptr<MediaStreamAudioBus> output_bus_;

  // Indicates whether the audio processor playout signal has ever had
  // asymmetric left and right channel content.
  bool assume_upmixed_mono_playout_ = true;

  // These are mutated on the main render thread in OnCaptureFormatChanged().
  // The caller guarantees this does not run concurrently with accesses on the
  // capture audio thread.
  media::AudioParameters input_format_;
  media::AudioParameters output_format_;

  // TODO(crbug.com/704136): Replace with Member at some point.
  scoped_refptr<WebRtcAudioDeviceImpl> playout_data_source_;

  // Task runner for the main render thread.
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner_;

  // Used to DCHECK that some methods are called on the capture audio thread.
  THREAD_CHECKER(capture_thread_checker_);
  // Used to DCHECK that some methods are called on the render audio thread.
  THREAD_CHECKER(render_thread_checker_);

  // Flag to enable stereo channel mirroring.
  bool audio_mirroring_;

  // Communication with browser for AEC dump.
  std::unique_ptr<AecDumpAgentImpl> aec_dump_agent_impl_;

  // Flag to avoid executing Stop() more than once.
  bool stopped_;

  // Counters to avoid excessively logging errors in OnPlayoutData.
  size_t unsupported_buffer_size_log_count_ = 0;
  size_t apm_playout_error_code_log_count_ = 0;
  size_t large_delay_log_count_ = 0;

  // Flag indicating whether capture multi channel processing should be active.
  const bool use_capture_multi_channel_processing_;

  // Observed maximum number of preferred output channels. Used for not
  // performing audio processing on more channels than the sinks are interested
  // in. The value is a maximum over time and can increase but never decrease.
  // If use_capture_multi_channel_processing_ is true, Audio Processing Module
  // (APM) will output max_num_preferred_output_channels_ channels as long as it
  // does not exceed the number of channels of the output format.
  int max_num_preferred_output_channels_ = 1;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamAudioProcessor);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_AUDIO_PROCESSOR_H_
