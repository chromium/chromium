// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_AAUDIO_STREAM_WRAPPER_H_
#define MEDIA_AUDIO_ANDROID_AAUDIO_STREAM_WRAPPER_H_

#include <aaudio/AAudio.h>

#include "base/android/requires_api.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"

// For use with REQUIRES_ANDROID_API() and __builtin_available().
// We need APIs that weren't added until API Level 28. Also, AAudio crashes
// on P, so only consider Q and above.
#define AAUDIO_MIN_API 29

namespace media {

class AAudioDestructionHelper;

// Small wrapper around AAudioStream which handles its lifetime.
class REQUIRES_ANDROID_API(AAUDIO_MIN_API) AAudioStreamWrapper {
 public:
  enum class StreamType {
    kInput,
    kOutput,
  };

  // Interface to report errors or provide/request audio data.
  // Called on AAudio's realtime audio thread.
  // Do not perform blocking operations from these callback, or call
  // Open()/Start()/Stop()/Close() without jumping to a different thread first.
  class DataCallback {
   public:
    virtual ~DataCallback() = default;

    virtual bool OnAudioDataRequested(void* audio_data, int32_t num_frames) = 0;
    virtual void OnError() = 0;
    virtual void OnDeviceChange() = 0;
  };

  AAudioStreamWrapper(DataCallback* callback,
                      StreamType stream_type,
                      const AudioParameters& params,
                      aaudio_usage_t usage);

  AAudioStreamWrapper(const AAudioStreamWrapper&) = delete;
  AAudioStreamWrapper& operator=(const AAudioStreamWrapper&) = delete;

  ~AAudioStreamWrapper();

  // Manage the underlying stream's lifetime.
  // Returns whether the operation was successful.
  bool Open();
  bool Start();
  bool Stop();
  void Close();  // No other calls should be made after this one.

  // Called on AAudio's realtime thread. Forwards calls to `callback_`.
  aaudio_data_callback_result_t OnAudioDataRequested(void* audio_data,
                                                     int32_t num_frames);
  void OnStreamError(aaudio_result_t error);

  // Returns the amount of unplayed audio relative to |delay_timestamp|.
  base::TimeDelta GetOutputDelay(base::TimeTicks delay_timestamp);

  // Returns the time at which the next sample read from `aaudio_stream_` was
  // recorded.
  base::TimeTicks GetCaptureTimestamp();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  const AudioParameters params_;

  // Whether this class is using an input or an output stream.
  StreamType stream_type_;

  aaudio_usage_t usage_;
  aaudio_performance_mode_t performance_mode_;

  const raw_ptr<DataCallback> callback_;

  bool is_closed_ = false;

  raw_ptr<AAudioStream> aaudio_stream_ = nullptr;

  // Constant used for calculating latency. Amount of nanoseconds per frame.
  const double ns_per_frame_;

  // Bound to the audio data callback. Outlives |this| in case the callbacks
  // continue after |this| is destroyed. See crbug.com/1183255.
  std::unique_ptr<AAudioDestructionHelper> destruction_helper_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_ANDROID_AAUDIO_STREAM_WRAPPER_H_
