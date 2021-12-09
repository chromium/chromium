// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/audio_processor.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_bus.h"
#include "services/audio/device_output_listener.h"

namespace audio {

class AudioProcessor::UmaLogger {
 public:
  UmaLogger(const std::string& device_id)
      : is_default_(media::AudioDeviceDescription::IsDefaultDevice(device_id)),
        start_(base::TimeTicks::Now()) {}

  UmaLogger(const UmaLogger&) = delete;
  UmaLogger& operator=(const UmaLogger&) = delete;

  ~UmaLogger() {
    base::UmaHistogramLongTimes(
        base::StrCat({"Media.Audio.OutputDeviceListener.Duration.",
                      ((is_default_) ? "Default" : "NonDefault")}),
        base::TimeTicks::Now() - start_);
  }

 private:
  const bool is_default_;
  base::TimeTicks start_;
};

AudioProcessor::AudioProcessor(DeviceOutputListener* device_output_listener,
                               LogCallback log_callback)
    : device_output_listener_(device_output_listener),
      log_callback_(std::move(log_callback)) {
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
  if (output_device_id_ == output_device_id ||
      (media::AudioDeviceDescription::IsDefaultDevice(output_device_id_) &&
       media::AudioDeviceDescription::IsDefaultDevice(output_device_id))) {
    return;
  }

  output_device_id_ = output_device_id;

  if (active_)
    StartListening();
}

void AudioProcessor::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(!active_);
  active_ = true;
  StartListening();
}

void AudioProcessor::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(active_);
  device_output_listener_->StopListening(this);
  log_callback_.Run("AudioProcessor: stop listening");
  active_ = false;
  uma_logger_.reset();
}

void AudioProcessor::OnPlayoutData(const media::AudioBus& audio_bus,
                                   int sample_rate,
                                   base::TimeDelta delay) {
  TRACE_EVENT2("audio", "AudioProcessor::OnData", " this ",
               static_cast<void*>(this), "delay", delay.InMillisecondsF());
}

void AudioProcessor::StartListening() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(active_);
  uma_logger_ = std::make_unique<UmaLogger>(output_device_id_);
  log_callback_.Run(base::StrCat(
      {"AudioProcessor: listening to output device: ", output_device_id_}));
  device_output_listener_->StartListening(this, output_device_id_);
}

}  // namespace audio
