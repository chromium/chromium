// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_AUDIO_PROCESSOR_H_
#define SERVICES_AUDIO_AUDIO_PROCESSOR_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "services/audio/reference_output.h"

namespace media {
class AudioBus;
}  // namespace media

namespace audio {
class DeviceOutputListener;

class AudioProcessor final : public ReferenceOutput::Listener {
 public:
  explicit AudioProcessor(DeviceOutputListener* device_output_listener);
  AudioProcessor(const AudioProcessor&) = delete;
  AudioProcessor& operator=(const AudioProcessor&) = delete;
  ~AudioProcessor() final;

  void SetOutputDeviceForAec(const std::string& output_device_id);
  void Start();
  void Stop();

 private:
  // Listener
  void OnPlayoutData(const media::AudioBus& audio_bus,
                     int sample_rate,
                     base::TimeDelta delay) final;

  SEQUENCE_CHECKER(owning_sequence_);
  bool active_ = false;
  std::string output_device_id_;
  raw_ptr<DeviceOutputListener> const device_output_listener_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_AUDIO_PROCESSOR_H_
