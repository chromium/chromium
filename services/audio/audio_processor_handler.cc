// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/audio_processor_handler.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_bus.h"
#include "services/audio/device_output_listener.h"

namespace audio {

class AudioProcessorHandler::UmaLogger {
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

AudioProcessorHandler::AudioProcessorHandler(
    DeviceOutputListener* device_output_listener,
    LogCallback log_callback)
    : device_output_listener_(device_output_listener),
      log_callback_(std::move(log_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(device_output_listener_);
}

AudioProcessorHandler::~AudioProcessorHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (active_)
    Stop();
}

void AudioProcessorHandler::SetOutputDeviceForAec(
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

void AudioProcessorHandler::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(!active_);
  active_ = true;
  StartListening();
}

void AudioProcessorHandler::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(active_);
  device_output_listener_->StopListening(this);
  log_callback_.Run("AudioProcessorHandler: stop listening");
  active_ = false;
  uma_logger_.reset();
}

void AudioProcessorHandler::OnPlayoutData(const media::AudioBus& audio_bus,
                                          int sample_rate,
                                          base::TimeDelta delay) {
  TRACE_EVENT2("audio", "AudioProcessorHandler::OnData", " this ",
               static_cast<void*>(this), "delay", delay.InMillisecondsF());
}

void AudioProcessorHandler::StartListening() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(active_);
  uma_logger_ = std::make_unique<UmaLogger>(output_device_id_);
  log_callback_.Run(
      base::StrCat({"AudioProcessorHandler: listening to output device: ",
                    output_device_id_}));
  device_output_listener_->StartListening(this, output_device_id_);
}

}  // namespace audio
