// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_APPLE_AUDIO_MANAGER_APPLE_H_
#define MEDIA_AUDIO_APPLE_AUDIO_MANAGER_APPLE_H_

#include "media/audio/apple/audio_io_stream_client.h"
#include "media/audio/audio_manager_base.h"

namespace media {

class AudioManagerApple : public AudioManagerBase, public AudioIOStreamClient {
 public:
  AudioManagerApple(const AudioManagerApple&) = delete;
  AudioManagerApple& operator=(const AudioManagerApple&) = delete;

  ~AudioManagerApple() override = default;

  // Apple platform specific implementations overridden by mac and ios.
  // Manage device capabilities for ambient noise reduction. These functionality
  // currently implemented on the Mac platform.
  virtual bool DeviceSupportsAmbientNoiseReduction(AudioDeviceID device_id) = 0;
  virtual bool SuppressNoiseReduction(AudioDeviceID device_id) = 0;
  virtual void UnsuppressNoiseReduction(AudioDeviceID device_id) = 0;

  // Returns the maximum microphone analog volume or 0.0 if device does not
  // have volume control.
  virtual double GetMaxInputVolume(AudioDeviceID device_id) = 0;

  // Sets the microphone analog volume, with range [0.0, 1.0] inclusive.
  virtual void SetInputVolume(AudioDeviceID device_id, double volume) = 0;

  // Returns the microphone analog volume, with range [0.0, 1.0] inclusive.
  virtual double GetInputVolume(AudioDeviceID device_id) = 0;

  // Returns the current muting state for the microphone.
  virtual bool IsInputMuted(AudioDeviceID device_id) = 0;

  // Check if delayed start for stream is needed.
  // Refer main:media/audio/mac/audio_manager_mac.h for more details.
  virtual bool ShouldDeferStreamStart() const = 0;

 protected:
  AudioManagerApple(std::unique_ptr<AudioThread> audio_thread,
                    AudioLogFactory* audio_log_factory);
};

}  // namespace media

#endif  // MEDIA_AUDIO_APPLE_AUDIO_MANAGER_APPLE_H_
