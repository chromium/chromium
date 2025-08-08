// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_AAUDIO_BLUETOOTH_OUTPUT_H_
#define MEDIA_AUDIO_ANDROID_AAUDIO_BLUETOOTH_OUTPUT_H_

#include <aaudio/AAudio.h>

#include "base/android/requires_api.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "media/audio/android/aaudio_output.h"
#include "media/audio/android/audio_device.h"
#include "media/audio/android/muteable_audio_output_stream.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioManagerAndroid;

// Class which uses the AAudio library to playback output to a Bluetooth Classic
// device supporting both A2DP and SCO. It wraps two instances of
// `AAudioOutputStream`, each only functional for one of the two protocols, such
// that the correct of the two streams can be selected to play audio at a given
// time.
class AAudioBluetoothOutputStream : public MuteableAudioOutputStream {
 public:
  AAudioBluetoothOutputStream(
      AudioManagerAndroid& manager,
      const AudioParameters& params,
      android::AudioDevice device,
      bool use_sco_device,
      aaudio_usage_t usage,
      AmplitudePeakDetector::PeakDetectedCB peak_detected_cb);

  ~AAudioBluetoothOutputStream() override;

  // Implementation of MuteableAudioOutputStream.
  bool Open() override;
  void Close() override;
  void Start(AudioSourceCallback* callback) override;
  void Stop() override;
  void Flush() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;
  void SetMute(bool muted) override;

  // Set which inner stream to use for playing audio. If `use_sco_device` is
  // `true`, the SCO stream will be used; otherwise, the A2DP stream will be
  // used.
  void SetUseSco(bool use_sco_device);

 private:
  // Returns the inner stream which is currently meant to be used for audio data
  // callbacks, as determined by whether or not Bluetooth SCO is enabled.
  AAudioOutputStream& GetActiveInnerStream() const;

  const raw_ref<AudioManagerAndroid> manager_;

  bool use_sco_;

  std::unique_ptr<AAudioOutputStream> inner_a2dp_stream_;
  std::unique_ptr<AAudioOutputStream> inner_sco_stream_;

  raw_ptr<AudioSourceCallback> callback_ = nullptr;
};

}  // namespace media

#endif  // MEDIA_AUDIO_ANDROID_AAUDIO_BLUETOOTH_OUTPUT_H_
