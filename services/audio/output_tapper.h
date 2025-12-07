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
#include "services/audio/reference_signal_provider.h"

namespace audio {

class OutputTapper {
 public:
  using LogCallback = base::RepeatingCallback<void(std::string_view)>;

  OutputTapper(
      std::unique_ptr<ReferenceSignalProvider> reference_signal_provider,
      ReferenceOutput::Listener* listener,
      LogCallback log_callback);
  OutputTapper(const OutputTapper&) = delete;
  OutputTapper& operator=(const OutputTapper&) = delete;
  ~OutputTapper();

  void SetOutputDeviceForAec(const std::string& output_device_id);
  ReferenceSignalProvider::ReferenceOpenOutcome Start();
  void Stop();

 private:
  ReferenceSignalProvider::ReferenceOpenOutcome StartListening();

  SEQUENCE_CHECKER(owning_sequence_);
  bool active_ = false;
  std::string output_device_id_;
  std::unique_ptr<ReferenceSignalProvider> const reference_signal_provider_;
  raw_ptr<ReferenceOutput::Listener> const listener_;
  const LogCallback log_callback_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_OUTPUT_TAPPER_H_
