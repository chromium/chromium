// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_INPUT_GLITCH_COUNTER_H_
#define SERVICES_AUDIO_INPUT_GLITCH_COUNTER_H_

#include <cstddef>
#include <vector>
#include "base/functional/callback.h"
#include "base/time/time.h"
namespace audio {

// Class for logging audio glitch data at 10 second intervals, for use by
// InputSyncWriter. Upon destruction, logs how much data was dropped to
// |log_callback|.
class InputGlitchCounter {
 public:
  explicit InputGlitchCounter(
      base::RepeatingCallback<void(const std::string&)> log_callback);
  virtual ~InputGlitchCounter();

  InputGlitchCounter(const InputGlitchCounter&) = delete;
  InputGlitchCounter& operator=(const InputGlitchCounter&) = delete;

  // Report either a successful attempt to write data to shared memory, in which
  // case |dropped_data| is false, or that data was dropped for any reason
  // (typically because the fifo or socket buffer is full), in which case
  // |dropped_data| is true.
  virtual void ReportDroppedData(bool dropped_data);

  // Reports a call to InputSyncWriter::Write(), and whether or not the reader
  // met the read deadline.
  virtual void ReportMissedReadDeadline(bool missed_read_deadline);

 private:
  // Represents a sample of write statistics during a 10 second interval.
  struct Sample {
    // Counts the number of times InputSyncWriter::Write() was called and the
    // shared memory was full.
    size_t missed_read_deadline_count_ = 0;

    // Counts the number times data was dropped, due to either the fifo or the
    // socket buffer being full.
    size_t dropped_data_count_ = 0;
  };

  void UploadCompleteSamples();

  // The number of calls to ReportMissedReadDeadline(), which corresponds to the
  // number of calls to InputSyncWriter::Write().
  size_t write_count_ = 0;

  // The current, incomplete sample. Moved to complete_samples_ every 10
  // seconds.
  Sample current_sample_;

  // Samples that are ready to be uploaded once we are sure that they do not
  // contain trailing errors.
  std::vector<Sample> complete_samples_;

  // Contains the data for the whole lifetime of the stream.
  struct GlobalSample : Sample {
    // Error counts that occur while the renderer process is being shut down,
    // that will be subtracted when calculating the final statistics. Only used
    // for calculating the statistics for streams of less than 10 seconds.
    size_t trailing_missed_read_deadline_count_ = 0;
    size_t trailing_dropped_data_count_ = 0;
  } global_sample_;

  base::RepeatingCallback<void(const std::string&)> log_callback_;

  // The number of calls to ReportMissedReadDeadline(), which corresponds to
  // InputSyncWriter::Write(), that constitute a sample period. For 10ms buffer
  // sizes this corresponds to 10 seconds.
  static constexpr int kSampleInterval = 1000;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_INPUT_GLITCH_COUNTER_H_
