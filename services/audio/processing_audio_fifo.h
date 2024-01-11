// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PROCESSING_AUDIO_FIFO_H_
#define SERVICES_AUDIO_PROCESSING_AUDIO_FIFO_H_

#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_annotations.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_parameters.h"
#include "services/audio/realtime_audio_thread.h"

namespace media {
class AudioBus;
}

namespace audio {

// ProcessingAudioFifo is a ring buffer, which offloads audio processing to its
// own dedicated real-time thread. It interacts with 3 sequences:
//   - An owning sequence, on which it is constructed, started, stopped and
//     destroyed.
//   - A capture sequence which calls PushData().
//   - A processing thread, which ProcessingAudioFifo owns, on which a
//     processing callback is called.
class ProcessingAudioFifo {
 public:
  using ProcessAudioCallback =
      base::RepeatingCallback<void(const media::AudioBus&,
                                   base::TimeTicks,
                                   double,
                                   bool,
                                   const media::AudioGlitchInfo&)>;

  using LogCallback = base::RepeatingCallback<void(std::string_view)>;

  // |processing_callback| will only be called back on the processing thread.
  ProcessingAudioFifo(const media::AudioParameters& input_params,
                      int fifo_size,
                      ProcessAudioCallback processing_callback,
                      LogCallback log_callback);

  // Note: This synchronously waits for |audio_processing_thread_.Stop()|.
  ~ProcessingAudioFifo();

  // Disallow copy and assign.
  ProcessingAudioFifo(const ProcessingAudioFifo&) = delete;
  ProcessingAudioFifo& operator=(const ProcessingAudioFifo&) = delete;

  // Adds data to the FIFO, waking up the processing thread in the process.
  // If the FIFO is full, new data will be dropped.
  // Called on the capture thread.
  void PushData(const media::AudioBus* audio_bus,
                base::TimeTicks capture_time,
                double volume,
                bool key_pressed,
                const media::AudioGlitchInfo& audio_glitch_info);

  // Starts the processing thread. Cannot be called more than once.
  void Start();

  // |fake_new_data_captured| will replace |new_data_captured_| in the
  // ProcessAudioLoop().
  void StartForTesting(base::WaitableEvent* fake_new_data_captured);

  // Adds a callback that will be run immediately after |processing_callback_|,
  // on the same sequences as |processing_callback_|.
  void AttachOnProcessedCallbackForTesting(
      base::RepeatingClosure on_processed_callback);

  int fifo_size() const { return fifo_size_; }

 private:
  friend class ProcessingAudioFifoTest;

  class StatsReporter;
  struct CaptureData;

  void StartInternal(base::WaitableEvent* new_data_captured,
                     base::Thread::Options options);
  void StopProcessingLoop();

  void ProcessAudioLoop(base::WaitableEvent* new_data_captured);

  CaptureData* GetDataAtIndex(int idx);

  base::Lock fifo_index_lock_;

  // Total number of segments written to |fifo_|.
  // Updated on capture thread. Read on capture and processing thread.
  int write_count_ GUARDED_BY(fifo_index_lock_) = 0;

  // Total number of segments read from the |fifo_|, and processed by
  // |processing_callback_|.
  // Updated on processing thread. Read on capture and processing thread.
  int read_count_ GUARDED_BY(fifo_index_lock_) = 0;

  // Pre-allocated circular buffer of captured audio data, used to handoff data
  // from the capture thread to the processing thread.
  const int fifo_size_;
  std::vector<CaptureData> fifo_;

  // Expected format of captured audio data.
  const media::AudioParameters input_params_;

  // Real-time audio processing thread, on which |processing_callback_| is
  // called.
  RealtimeAudioThread audio_processing_thread_;

  // Processes captured audio data. Only run on |audio_processing_thread_|.
  ProcessAudioCallback processing_callback_;

  base::AtomicFlag fifo_stopping_;

  // Event signaling that there is new audio data to process. Called from the
  // capture thread, and waited for on the processing thread.
  base::WaitableEvent new_data_captured_;

  std::unique_ptr<StatsReporter> stats_reporter_;

  SEQUENCE_CHECKER(owning_sequence_checker_);

  media::AudioGlitchInfo::Accumulator glitch_info_accumulator_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_PROCESSING_AUDIO_FIFO_H_
