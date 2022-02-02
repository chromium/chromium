// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_OUTPUT_TAPPER_H_
#define SERVICES_AUDIO_OUTPUT_TAPPER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "services/audio/reference_output.h"

namespace media {
class AudioBus;
}  // namespace media

namespace audio {
class DeviceOutputListener;

class OutputTapper final : public ReferenceOutput::Listener {
 public:
  using LogCallback = base::RepeatingCallback<void(base::StringPiece)>;

  OutputTapper(DeviceOutputListener* device_output_listener,
               LogCallback log_callback);
  OutputTapper(const OutputTapper&) = delete;
  OutputTapper& operator=(const OutputTapper&) = delete;
  ~OutputTapper() final;

  void SetOutputDeviceForAec(const std::string& output_device_id);
  void Start();
  void Stop();

 private:
  class UmaLogger;
  // Listener
  void OnPlayoutData(const media::AudioBus& audio_bus,
                     int sample_rate,
                     base::TimeDelta delay) final;

  void StartListening();

  SEQUENCE_CHECKER(owning_sequence_);
  bool active_ = false;
  std::string output_device_id_;
  raw_ptr<DeviceOutputListener> const device_output_listener_;
  const LogCallback log_callback_;
  std::unique_ptr<UmaLogger> uma_logger_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_OUTPUT_TAPPER_H_
