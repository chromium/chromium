// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_AUDIO_PROCESSOR_H_
#define MEDIA_WEBRTC_AUDIO_PROCESSOR_H_

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_processing.h"
#include "media/base/audio_parameters.h"
#include "media/webrtc/audio_delay_stats_reporter.h"
#include "media/webrtc/audio_processor_controls.h"
#include "media/webrtc/echo_information.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"
#include "third_party/webrtc/modules/audio_processing/typing_detection.h"
#include "third_party/webrtc/rtc_base/task_queue.h"

namespace media {

// The AudioProcessor wraps the WebRTC AudioProcessingModule. It also provides
// stereo channel mirroring. For now, it will only run in the audio service when
// run outside the browser process. As such, it does not support Android
// specific configuration, like enabling mobile mode.
class COMPONENT_EXPORT(MEDIA_WEBRTC) AudioProcessor final
    : public AudioProcessorControls {
 public:
  // |audio_parameters| describe the format to use, and |settings| what
  // processing to perform. The WebRTC APM will resample internally if
  // necessary. AudioProcessor uses the same format for input and output. Audio
  // must be provided in 10ms chunks.
  AudioProcessor(const AudioParameters& audio_parameters,
                 const AudioProcessingSettings& settings);

  ~AudioProcessor() final;

  // The result of a call to ProcessCapture. |audio| is allocated and owned by
  // the AudioProcessor. It is valid until the next call to ProcessCapture or
  // until destruction of the AudioProcessor.
  struct COMPONENT_EXPORT(MEDIA_WEBRTC) ProcessingResult {
    ProcessingResult(const AudioBus& audio, base::Optional<double> new_volume);
    ProcessingResult(const ProcessingResult& b);
    ~ProcessingResult();

    const AudioBus& audio;
    base::Optional<double> new_volume;
  };

  ProcessingResult ProcessCapture(const AudioBus& source,
                                  base::TimeTicks capture_time,
                                  double volume,
                                  bool key_pressed);

  void AnalyzePlayout(const AudioBus& audio,
                      const AudioParameters& parameters,
                      base::TimeTicks playout_time);

  void UpdateInternalStats();

  void set_has_reverse_stream(bool has_reverse_stream) {
    has_reverse_stream_ = has_reverse_stream;
  }

  // AudioProcessorControls implementation.
  void GetStats(GetStatsCB callback) override;
  void StartEchoCancellationDump(base::File file) override;
  void StopEchoCancellationDump() override;

 private:
  friend class WebRtcAudioProcessorTest;

  void InitializeAPM();

  // These functions are all part of ProcessCapture and assume that
  // |audio_processing_| is set.
  void UpdateDelayEstimate(base::TimeTicks capture_time);
  void UpdateAnalogLevel(double volume);
  void FeedDataToAPM(const AudioBus& source);
  void UpdateTypingDetected(bool key_pressed);
  base::Optional<double> GetNewVolumeFromAGC(double volume);

  const AudioParameters audio_parameters_;
  const AudioProcessingSettings settings_;

  // State related to interaction with APM.
  std::unique_ptr<webrtc::AudioProcessing> audio_processing_;
  std::unique_ptr<webrtc::TypingDetection> typing_detector_;
  std::atomic<bool> typing_detected_ = {false};
  std::atomic<base::TimeDelta> render_delay_ = {base::TimeDelta()};
  bool has_reverse_stream_ = false;

  // The APM writes the processed data here.
  std::unique_ptr<AudioBus> output_bus_;
  std::vector<float*> output_ptrs_;

  // For reporting audio delay stats.
  AudioDelayStatsReporter audio_delay_stats_reporter_;

  // Low-priority task queue for doing AEC dump recordings. It has to
  // out-live |audio_processing_| and be created/destroyed from the same
  // thread.
  std::unique_ptr<rtc::TaskQueue> worker_queue_;

  EchoInformation echo_information_;

  DISALLOW_COPY_AND_ASSIGN(AudioProcessor);
};

}  // namespace media

#endif  // MEDIA_WEBRTC_AUDIO_PROCESSOR_H_
