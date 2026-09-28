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
#include "media/audio/android/audio_device.h"
#include "media/audio/audio_io.h"
#include "media/base/amplitude_peak_detector.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_push_fifo.h"
#include "media/base/audio_timestamp_helper.h"

namespace media {

class AudioManagerAndroid;
class AAudioInputDiscontinuityReporter;

// Class which uses the AAudio library to record input.
class AAudioInputStream : public AudioInputStream,
                          public AAudioStreamWrapper::DataCallback {
 public:
  AAudioInputStream(AudioManagerAndroid* manager,
                    const AudioParameters& params,
                    android::AudioDevice device);

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
  bool OnAudioDataRequested(base::span<float> audio_data) override;
  void OnError() override;
  void OnDeviceChange() override;

  // Returns if `device_` explicitly requests Bluetooth SCO type.
  bool IsExplicitlyRequestingBluetoothSco();
  // Returns the ID of the "actual" device the stream was opened with.
  std::optional<android::AudioDeviceId> GetActualDeviceId();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  void CreateStreamWrapper();
  void HandleDeviceChange();
  void OnFifoFilled(const AudioBus& output_bus, int frame_delay);
  void DeliverAudio(const AudioBus& audio_bus, base::TimeTicks capture_time);

  const raw_ptr<AudioManagerAndroid> audio_manager_;
  const AudioParameters params_;
  const android::AudioDevice device_;

  AmplitudePeakDetector peak_detector_;

  std::unique_ptr<AAudioStreamWrapper> stream_wrapper_;

  std::unique_ptr<AudioBus> audio_bus_;

  // Used for handling variable-sized callbacks.
  std::unique_ptr<AudioPushFifo> push_fifo_;
  // A wrapper over `audio_bus_` to pass partial data to `push_fifo_`.
  std::unique_ptr<AudioBus> wrapper_bus_;

  // Used to calculate delay when audio data received spans multiple
  // `OnFifoFilled` calls.
  AudioTimestampHelper timestamp_helper_;

  std::unique_ptr<AAudioInputDiscontinuityReporter> discontinuity_reporter_;

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
