// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_AAUDIO_OUTPUT_H_
#define MEDIA_AUDIO_ANDROID_AAUDIO_OUTPUT_H_

#include <aaudio/AAudio.h>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "media/audio/android/muteable_audio_output_stream.h"
#include "media/base/audio_parameters.h"

namespace media {

class AAudioDestructionHelper;
class AudioManagerAndroid;

class AAudioOutputStream : public MuteableAudioOutputStream {
 public:
  AAudioOutputStream(AudioManagerAndroid* manager,
                     const AudioParameters& params,
                     aaudio_usage_t usage);

  AAudioOutputStream(const AAudioOutputStream&) = delete;
  AAudioOutputStream& operator=(const AAudioOutputStream&) = delete;

  ~AAudioOutputStream() override;

  // Implementation of MuteableAudioOutputStream.
  bool Open() override;
  void Close() override;
  void Start(AudioSourceCallback* callback) override;
  void Stop() override;
  void Flush() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;
  void SetMute(bool muted) override;

  // Public callbacks.
  aaudio_data_callback_result_t OnAudioDataRequested(void* audio_data,
                                                     int32_t num_frames);
  void OnStreamError(aaudio_result_t error);

 private:
  // Returns the amount of unplayed audio relative to |delay_timestamp|. See the
  // definition for AudioOutputStream::AudioSourceCallback::OnMoreData() for
  // more information on these terms.
  base::TimeDelta GetDelay(base::TimeTicks delay_timestamp);

  THREAD_CHECKER(thread_checker_);

  const raw_ptr<AudioManagerAndroid> audio_manager_;
  const AudioParameters params_;

  aaudio_usage_t usage_;
  aaudio_performance_mode_t performance_mode_;

  // Constant used for calculating latency. Amount of nanoseconds per frame.
  const double ns_per_frame_;

  std::unique_ptr<AudioBus> audio_bus_;

  AAudioStream* aaudio_stream_ = nullptr;

  // Bound to the audio data callback. Outlives |this| in case the callbacks
  // continue after |this| is destroyed. See crbug.com/1183255.
  std::unique_ptr<AAudioDestructionHelper> destruction_helper_;

  // Lock protects all members below which may be read concurrently from the
  // audio manager thread and the OS provided audio thread.
  base::Lock lock_;

  raw_ptr<AudioSourceCallback> callback_ GUARDED_BY(lock_) = nullptr;
  bool muted_ GUARDED_BY(lock_) = false;
  double volume_ GUARDED_BY(lock_) = 1.0;
  bool device_changed_ GUARDED_BY(lock_) = false;
};

}  // namespace media

#endif  // MEDIA_AUDIO_ANDROID_AAUDIO_OUTPUT_H_
