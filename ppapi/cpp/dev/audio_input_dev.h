// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_AUDIO_INPUT_DEV_H_
#define PPAPI_CPP_DEV_AUDIO_INPUT_DEV_H_

#include <stdint.h>

#include <vector>

#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/cpp/audio_config.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/device_ref_dev.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class InstanceHandle;

class AudioInput_Dev : public Resource {
 public:
  /// An empty constructor for an AudioInput resource.
  AudioInput_Dev();

  /// Constructor to create an audio input resource.
  explicit AudioInput_Dev(const InstanceHandle& instance);

  virtual ~AudioInput_Dev();

  /// Static function for determining whether the browser supports the required
  /// AudioInput interface.
  ///
  /// @return true if the interface is available, false otherwise.
  static bool IsAvailable();

  int32_t EnumerateDevices(
      const CompletionCallbackWithOutput<std::vector<DeviceRef_Dev> >&
          callback);

  int32_t MonitorDeviceChange(PP_MonitorDeviceChangeCallback callback,
                              void* user_data);

  /// If |device_ref| is null (i.e., is_null() returns true), the default device
  /// will be used.
  ///
  /// Requires <code>PPB_AudioInput_Dev</code> version 0.4 or up.
  int32_t Open(const DeviceRef_Dev& device_ref,
               const AudioConfig& config,
               PPB_AudioInput_Callback audio_input_callback,
               void* user_data,
               const CompletionCallback& callback);

  /// Requires <code>PPB_AudioInput_Dev</code> version 0.3.
  int32_t Open(const DeviceRef_Dev& device_ref,
               const AudioConfig& config,
               PPB_AudioInput_Callback_0_3 audio_input_callback_0_3,
               void* user_data,
               const CompletionCallback& callback);

  bool StartCapture();
  bool StopCapture();
  void Close();
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_AUDIO_INPUT_DEV_H_
