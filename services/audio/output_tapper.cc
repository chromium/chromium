// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/output_tapper.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_bus.h"
#include "services/audio/device_output_listener.h"

namespace audio {

OutputTapper::OutputTapper(DeviceOutputListener* device_output_listener,
                           ReferenceOutput::Listener* listener,
                           LogCallback log_callback)
    : device_output_listener_(device_output_listener),
      listener_(listener),
      log_callback_(std::move(log_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(device_output_listener_);
  DCHECK(listener_);
  DCHECK(log_callback_);
}

OutputTapper::~OutputTapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (active_)
    Stop();
}

void OutputTapper::SetOutputDeviceForAec(const std::string& output_device_id) {
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

void OutputTapper::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(!active_);
  active_ = true;
  StartListening();
}

void OutputTapper::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(active_);
  device_output_listener_->StopListening(listener_);
  log_callback_.Run("OutputTapper: stop listening");
  active_ = false;
}

void OutputTapper::StartListening() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(active_);
  log_callback_.Run(base::StrCat(
      {"OutputTapper: listening to output device: ", output_device_id_}));
  device_output_listener_->StartListening(listener_, output_device_id_);
}

}  // namespace audio
