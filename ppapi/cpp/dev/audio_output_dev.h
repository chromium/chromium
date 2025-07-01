// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_AUDIO_OUTPUT_DEV_H_
#define PPAPI_CPP_DEV_AUDIO_OUTPUT_DEV_H_

#include <stdint.h>

#include <vector>

#include "ppapi/c/dev/ppb_audio_output_dev.h"
#include "ppapi/cpp/audio_config.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/device_ref_dev.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class InstanceHandle;

class AudioOutput_Dev : public Resource {
 public:
  // An empty constructor for an AudioOutput resource.
  AudioOutput_Dev();

  // Constructor to create an audio output resource.
  explicit AudioOutput_Dev(const InstanceHandle& instance);

  virtual ~AudioOutput_Dev();

  // Static function for determining whether the browser supports the required
  // AudioOutput interface.
  //
  // @return true if the interface is available, false otherwise.
  static bool IsAvailable();

  int32_t EnumerateDevices(
      const CompletionCallbackWithOutput<std::vector<DeviceRef_Dev> >& cb);

  int32_t MonitorDeviceChange(PP_MonitorDeviceChangeCallback callback,
                              void* user_data);

  // If |device_ref| is null (i.e., is_null() returns true), the default device
  // will be used.
  int32_t Open(const DeviceRef_Dev& device_ref,
               const AudioConfig& config,
               PPB_AudioOutput_Callback audio_output_callback,
               void* user_data,
               const CompletionCallback& callback);

  // Getter function for returning the internal <code>PPB_AudioConfig</code>
  // struct.
  //
  // @return A mutable reference to the PPB_AudioConfig struct.
  AudioConfig& config() { return config_; }

  // Getter function for returning the internal <code>PPB_AudioConfig</code>
  // struct.
  //
  // @return A const reference to the internal <code>PPB_AudioConfig</code>
  // struct.
  const AudioConfig& config() const { return config_; }

  // StartPlayback() starts playback of audio.
  //
  // @return true if successful, otherwise false.
  bool StartPlayback();

  // StopPlayback stops playback of audio.
  //
  // @return true if successful, otherwise false.
  bool StopPlayback();

  // Close closes the audio output device.
  void Close();

 private:
  AudioConfig config_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_AUDIO_OUTPUT_DEV_H_
