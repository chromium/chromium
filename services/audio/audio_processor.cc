// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/audio_processor.h"

#include "base/logging.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_bus.h"
#include "services/audio/device_output_listener.h"

namespace audio {

AudioProcessor::AudioProcessor(DeviceOutputListener* device_output_listener)
    : device_output_listener_(device_output_listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(device_output_listener_);
}

AudioProcessor::~AudioProcessor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (active_)
    Stop();
}

void AudioProcessor::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  output_device_id_ = output_device_id;

  if (active_)
    device_output_listener_->StartListening(this, output_device_id_);
}

void AudioProcessor::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(!active_);
  active_ = true;
  device_output_listener_->StartListening(this, output_device_id_);
}

void AudioProcessor::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(active_);
  device_output_listener_->StopListening(this);
  active_ = false;
}

void AudioProcessor::OnPlayoutData(const media::AudioBus& audio_bus,
                                   int sample_rate,
                                   base::TimeDelta delay) {
  TRACE_EVENT2("audio", "AudioProcessor::OnData", " this ",
               static_cast<void*>(this), "delay", delay.InMillisecondsF());
}

}  // namespace audio
