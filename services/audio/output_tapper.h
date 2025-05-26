// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_OUTPUT_TAPPER_H_
#define SERVICES_AUDIO_OUTPUT_TAPPER_H_

#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "services/audio/reference_output.h"

namespace audio {
class DeviceOutputListener;

class OutputTapper {
 public:
  using LogCallback = base::RepeatingCallback<void(std::string_view)>;

  OutputTapper(DeviceOutputListener* device_output_listener,
               ReferenceOutput::Listener* listener,
               LogCallback log_callback);
  OutputTapper(const OutputTapper&) = delete;
  OutputTapper& operator=(const OutputTapper&) = delete;
  ~OutputTapper();

  void SetOutputDeviceForAec(const std::string& output_device_id);
  void Start();
  void Stop();

 private:
  void StartListening();

  SEQUENCE_CHECKER(owning_sequence_);
  bool active_ = false;
  std::string output_device_id_;
  raw_ptr<DeviceOutputListener> const device_output_listener_;
  raw_ptr<ReferenceOutput::Listener> const listener_;
  const LogCallback log_callback_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_OUTPUT_TAPPER_H_
