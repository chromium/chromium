// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_AUDIO_PROCESSOR_H_
#define CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_AUDIO_PROCESSOR_H_

#include <memory>

#include "base/atomicops.h"
#include "base/files/file.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/common/media_stream_request.h"
#include "content/renderer/media/stream/aec_dump_message_filter.h"
#include "content/renderer/media/stream/media_stream_audio_processor_options.h"
#include "content/renderer/media/webrtc/webrtc_audio_device_impl.h"
#include "media/base/audio_converter.h"
#include "media/webrtc/audio_delay_stats_reporter.h"
#include "third_party/webrtc/api/mediastreaminterface.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"
#include "third_party/webrtc/rtc_base/task_queue.h"

namespace media {
class AudioBus;
class AudioParameters;
class EchoInformation;
}  // namespace media

namespace webrtc {
class TypingDetection;
}

namespace content {

class MediaStreamAudioBus;
class MediaStreamAudioFifo;

using webrtc::AudioProcessorInterface;

// This class owns an object of webrtc::AudioProcessing which contains signal
// processing components like AGC, AEC and NS. It enables the components based
// on the getUserMedia constraints, processes the data and outputs it in a unit
// of 10 ms data chunk.
class CONTENT_EXPORT MediaStreamAudioProcessor
    : public WebRtcPlayoutDataSource::Sink,
      public AudioProcessorInterface,
      public AecDumpMessageFilter::AecDumpDelegate {
 public:
  // |playout_data_source| is used to register this class as a sink to the
  // WebRtc playout data for processing AEC. If clients do not enable AEC,
  // |playout_data_source| won't be used.
  //
  // Threading note: The constructor assumes it is being run on the main render
  // thread.
  MediaStreamAudioProcessor(const AudioProcessingProperties& properties,
                            WebRtcPlayoutDataSource* playout_data_source);

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
  // |capture_delay| is an adjustment on the |capture_delay| value provided in
  // the last call to PushCaptureData().
  // |new_volume| receives the new microphone volume from the AGC.
  // The new microphone volume range is [0, 255], and the value will be 0 if
  // the microphone volume should not be adjusted.
  // Called on the capture audio thread.
  bool ProcessAndConsumeData(
      int volume,
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
  bool has_audio_processing() const { return audio_processing_ != NULL; }

  // AecDumpMessageFilter::AecDumpDelegate implementation.
  // Called on the main render thread.
  void OnAecDumpFile(const IPC::PlatformFileForTransit& file_handle) override;
  void OnDisableAecDump() override;
  void OnIpcClosing() override;

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
                           GetAecDumpMessageFilter);

  // WebRtcPlayoutDataSource::Sink implementation.
  void OnPlayoutData(media::AudioBus* audio_bus,
                     int sample_rate,
                     int audio_delay_milliseconds) override;
  void OnPlayoutDataSourceChanged() override;
  void OnRenderThreadChanged() override;

  // webrtc::AudioProcessorInterface implementation.
  // This method is called on the libjingle thread.
  void GetStats(AudioProcessorStats* stats) override;

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
  int ProcessData(const float* const* process_ptrs,
                  int process_frames,
                  base::TimeDelta capture_delay,
                  int volume,
                  bool key_pressed,
                  float* const* output_ptrs);

  // Update AEC stats. Called on the main render thread.
  void UpdateAecStats();

  // Cached value for the render delay latency. This member is accessed by
  // both the capture audio thread and the render audio thread.
  base::subtle::Atomic32 render_delay_ms_;

  // For reporting audio delay stats.
  media::AudioDelayStatsReporter audio_delay_stats_reporter_;

  // Low-priority task queue for doing AEC dump recordings. It has to
  // out-live audio_processing_ and be created/destroyed from the same
  // thread.
  std::unique_ptr<rtc::TaskQueue> worker_queue_;

  // Module to handle processing and format conversion.
  std::unique_ptr<webrtc::AudioProcessing> audio_processing_;

  // FIFO to provide 10 ms capture chunks.
  std::unique_ptr<MediaStreamAudioFifo> capture_fifo_;
  // Receives processing output.
  std::unique_ptr<MediaStreamAudioBus> output_bus_;

  // These are mutated on the main render thread in OnCaptureFormatChanged().
  // The caller guarantees this does not run concurrently with accesses on the
  // capture audio thread.
  media::AudioParameters input_format_;
  media::AudioParameters output_format_;

  // Raw pointer to the WebRtcPlayoutDataSource, which is valid for the
  // lifetime of RenderThread.
  WebRtcPlayoutDataSource* playout_data_source_;

  // Task runner for the main render thread.
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner_;

  // Used to DCHECK that some methods are called on the capture audio thread.
  base::ThreadChecker capture_thread_checker_;
  // Used to DCHECK that some methods are called on the render audio thread.
  base::ThreadChecker render_thread_checker_;

  // Flag to enable stereo channel mirroring.
  bool audio_mirroring_;

  // Typing detector. |typing_detected_| is used to show the result of typing
  // detection. It can be accessed by the capture audio thread and by the
  // libjingle thread which calls GetStats().
  std::unique_ptr<webrtc::TypingDetection> typing_detector_;
  base::subtle::Atomic32 typing_detected_;

  // Communication with browser for AEC dump.
  scoped_refptr<AecDumpMessageFilter> aec_dump_message_filter_;

  // Flag to avoid executing Stop() more than once.
  bool stopped_;

  // Counters to avoid excessively logging errors in OnPlayoutData.
  size_t unsupported_buffer_size_log_count_ = 0;
  size_t apm_playout_error_code_log_count_ = 0;
  size_t large_delay_log_count_ = 0;

  // Object for logging UMA stats for echo information when the AEC is enabled.
  // Accessed on the main render thread.
  std::unique_ptr<media::EchoInformation> echo_information_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamAudioProcessor);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_AUDIO_PROCESSOR_H_
