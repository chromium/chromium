// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_AAUDIO_OUTPUT_H_
#define MEDIA_AUDIO_ANDROID_AAUDIO_OUTPUT_H_

#include <aaudio/AAudio.h>

#include "base/android/requires_api.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "media/audio/android/aaudio_stream_wrapper.h"
#include "media/audio/android/muteable_audio_output_stream.h"
#include "media/base/amplitude_peak_detector.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioManagerAndroid;

// Class which uses the AAudio library to playback output.
class REQUIRES_ANDROID_API(AAUDIO_MIN_API) AAudioOutputStream
    : public MuteableAudioOutputStream,
      public AAudioStreamWrapper::DataCallback {
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

  // AAudioStreamWrapper::DataCallback implementation.
  bool OnAudioDataRequested(void* audio_data, int32_t num_frames) override;
  void OnError() override;
  void OnDeviceChange() override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  const raw_ptr<AudioManagerAndroid> audio_manager_;
  const AudioParameters params_;

  AmplitudePeakDetector peak_detector_;

  std::unique_ptr<AudioBus> audio_bus_;

  AAudioStreamWrapper stream_wrapper_;

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
