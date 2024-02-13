// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_APPLE_AUDIO_MANAGER_APPLE_H_
#define MEDIA_AUDIO_APPLE_AUDIO_MANAGER_APPLE_H_

#include <AudioUnit/AudioUnit.h>

#include "media/audio/audio_manager_base.h"
#include "media/base/mac/channel_layout_util_mac.h"

#if BUILDFLAG(IS_MAC)
#include <CoreAudio/CoreAudio.h>
#else
#include "media/audio/ios/audio_private_api.h"
#endif

namespace media {

class AudioManagerApple : public AudioManagerBase {
 public:
  AudioManagerApple(const AudioManagerApple&) = delete;
  AudioManagerApple& operator=(const AudioManagerApple&) = delete;

  ~AudioManagerApple() override;

  // Retrieve the output channel layout from a given `audio_unit`, Return
  // nullptr if failed.
  static std::unique_ptr<ScopedAudioChannelLayout> GetOutputDeviceChannelLayout(
      AudioUnit audio_unit);

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

  // Retrieves the current hardware sample rate associated with a specified
  // device.
  virtual int HardwareSampleRateForDevice(AudioDeviceID device_id) = 0;

  // If successful, this function returns no error and populates the out
  // parameter `input_format` with a valid ASBD. Otherwise, an error status code
  // will be returned.
  virtual OSStatus GetInputDeviceStreamFormat(
      AudioUnit audio_unit,
      AudioStreamBasicDescription* input_format) = 0;

  // Changes the I/O buffer size for `device_id` if `desired_buffer_size` is
  // lower than the current device buffer size. The buffer size can also be
  // modified under other conditions. See comments in the corresponding cc-file
  // for more details.
  // Returns false if an error occurred.
  virtual bool MaybeChangeBufferSize(AudioDeviceID device_id,
                                     AudioUnit audio_unit,
                                     AudioUnitElement element,
                                     size_t desired_buffer_size) = 0;

#if BUILDFLAG(IS_MAC)
  virtual base::TimeDelta GetDeferStreamStartTimeout() const = 0;
  virtual void StopAmplitudePeakTrace() = 0;
#endif

 protected:
  AudioManagerApple(std::unique_ptr<AudioThread> audio_thread,
                    AudioLogFactory* audio_log_factory);
};

}  // namespace media

#endif  // MEDIA_AUDIO_APPLE_AUDIO_MANAGER_APPLE_H_
