// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_AAUDIO_INPUT_H_
#define MEDIA_AUDIO_ANDROID_AAUDIO_INPUT_H_

#include <aaudio/AAudio.h>

#include "base/android/requires_api.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "media/audio/android/aaudio_stream_wrapper.h"
#include "media/audio/audio_io.h"
#include "media/base/amplitude_peak_detector.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioManagerAndroid;

// Class which uses the AAudio library to record input.
class REQUIRES_ANDROID_API(AAUDIO_MIN_API) AAudioInputStream
    : public AudioInputStream,
      public AAudioStreamWrapper::DataCallback {
 public:
  AAudioInputStream(AudioManagerAndroid* manager,
                    const AudioParameters& params);

  AAudioInputStream(const AAudioInputStream&) = delete;
  AAudioInputStream& operator=(const AAudioInputStream&) = delete;

  ~AAudioInputStream() override;

  // Implementation of AudioInputStream.
  OpenOutcome Open() override;
  void Start(AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool SetAutomaticGainControl(bool enabled) override;
  bool GetAutomaticGainControl() override;
  bool IsMuted() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

  // AAudioStreamWrapper::DataCallback implementation.
  bool OnAudioDataRequested(void* audio_data, int32_t num_frames) override;
  void OnError() override;
  void OnDeviceChange() override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  void CreateStreamWrapper();
  void HandleDeviceChange();

  const raw_ptr<AudioManagerAndroid> audio_manager_;
  const AudioParameters params_;

  AmplitudePeakDetector peak_detector_;

  std::unique_ptr<AAudioStreamWrapper> stream_wrapper_;

  std::unique_ptr<AudioBus> audio_bus_;

  base::RepeatingClosure handle_device_change_on_main_sequence_;

  // Lock protects all members below which may be read concurrently from the
  // audio manager thread and the OS provided audio thread.
  base::Lock lock_;
  raw_ptr<AudioInputCallback> callback_ GUARDED_BY(lock_) = nullptr;
  bool error_during_device_change_ GUARDED_BY(lock_) = false;

  base::WeakPtrFactory<AAudioInputStream> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_AUDIO_ANDROID_AAUDIO_INPUT_H_
