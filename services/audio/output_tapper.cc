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
#include "services/audio/reference_signal_provider.h"

namespace audio {

OutputTapper::OutputTapper(
    std::unique_ptr<ReferenceSignalProvider> reference_signal_provider,
    ReferenceOutput::Listener* listener,
    LogCallback log_callback)
    : reference_signal_provider_(std::move(reference_signal_provider)),
      listener_(listener),
      log_callback_(std::move(log_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(reference_signal_provider_);
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

ReferenceSignalProvider::ReferenceOpenOutcome OutputTapper::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(!active_);
  active_ = true;
  return StartListening();
}

void OutputTapper::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(active_);
  reference_signal_provider_->StopListening(listener_);
  log_callback_.Run("OutputTapper: stop listening");
  active_ = false;
}

ReferenceSignalProvider::ReferenceOpenOutcome OutputTapper::StartListening() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(active_);
  log_callback_.Run(base::StrCat(
      {"OutputTapper: listening to output device: ", output_device_id_}));
  return reference_signal_provider_->StartListening(listener_,
                                                    output_device_id_);
}

}  // namespace audio
