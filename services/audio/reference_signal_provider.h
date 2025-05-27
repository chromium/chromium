// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_REFERENCE_SIGNAL_PROVIDER_H_
#define SERVICES_AUDIO_REFERENCE_SIGNAL_PROVIDER_H_

#include <string>

#include "services/audio/reference_output.h"

namespace audio {

// Interface to start/stop listening to a device's reference output.
class ReferenceSignalProvider {
 public:
  virtual ~ReferenceSignalProvider() = default;

  // Starts listening to `device_id`'s output. Can be called multiple times
  // without calling StopListening(); each new call will replace which device
  // `listener` is listening to. Depending on implementation, additional devices
  // may also be included in the reference signal (as is the case with the
  // provider from LoopbackReferenceManager)
  //
  // `device_id` is expected to be a physical device ID, or the default device
  // ID, as defined by media::AudioDeviceDescription::IsDefaultDevice().
  //
  // If ever `device_id`'s validity changes (after disconnecting/reconnecting a
  // device), `listener` might start/stop receiving OnPlayoutData() calls.
  virtual void StartListening(ReferenceOutput::Listener* listener,
                              const std::string& device_id) = 0;

  // Stop `listener` from receiving its current device's reference output.
  // Must be called when `listener` no longer wants to receive data (e.g.
  // before it is destroyed). StartListening() must have been called.
  virtual void StopListening(ReferenceOutput::Listener* listener) = 0;
};

class ReferenceSignalProviderFactory {
 public:
  virtual ~ReferenceSignalProviderFactory() = default;

  virtual std::unique_ptr<ReferenceSignalProvider>
  GetReferenceSignalProvider() = 0;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_REFERENCE_SIGNAL_PROVIDER_H_
