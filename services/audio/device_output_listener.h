// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_DEVICE_OUTPUT_LISTENER_H_
#define SERVICES_AUDIO_DEVICE_OUTPUT_LISTENER_H_

#include <string>

#include "services/audio/reference_output.h"

namespace audio {

// Interface to start/stop listening to the reference output of a specific
// device. The |device_id| is expected to be a physical device ID, or the
// default device ID, as defined by
// media::AudioDeviceDescription::IsDefaultDevice().
class DeviceOutputListener {
 public:
  virtual ~DeviceOutputListener() = default;

  // Stops listening to a device's reference output.
  // In the case where |device_id| is not valid, |listener| must still
  // be removed by a call to StopListener(). Additionally, if ever |device_id|
  // becomes valid again (e.g. after re-plugging in a device), |listener| might
  // start receiving OnPlayoutData() calls again.
  virtual void StartListening(ReferenceOutput::Listener* listener,
                              const std::string& device_id) = 0;

  // Stops listening to a device's reference output.
  // StartListening(|listener|) must have already been called.
  virtual void StopListening(ReferenceOutput::Listener* listener,
                             const std::string& device_id) = 0;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_DEVICE_OUTPUT_LISTENER_H_
