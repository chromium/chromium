// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/aaudio_bluetooth_output.h"

#include <aaudio/AAudio.h>

#include "media/audio/android/aaudio_output.h"
#include "media/audio/android/audio_device.h"
#include "media/audio/android/audio_manager_android.h"
#include "media/base/audio_parameters.h"

namespace media {

AAudioBluetoothOutputStream::AAudioBluetoothOutputStream(
    AudioManagerAndroid& manager,
    const AudioParameters& params,
    android::AudioDevice device,
    bool use_sco_device,
    aaudio_usage_t usage,
    AmplitudePeakDetector::PeakDetectedCB peak_detected_cb)
    : manager_(manager), use_sco_(use_sco_device) {
  std::optional<android::AudioDevice> sco_device =
      device.GetAssociatedScoDevice();
  CHECK(sco_device.has_value());

  inner_a2dp_stream_ = std::make_unique<AAudioOutputStream>(
      nullptr, params, std::move(device), usage, peak_detected_cb);
  inner_sco_stream_ = std::make_unique<AAudioOutputStream>(
      nullptr, params, std::move(sco_device).value(), usage,
      std::move(peak_detected_cb));
}

AAudioBluetoothOutputStream::~AAudioBluetoothOutputStream() = default;

bool AAudioBluetoothOutputStream::Open() {
  return inner_a2dp_stream_->Open() && inner_sco_stream_->Open();
}

void AAudioBluetoothOutputStream::Close() {
  // These streams do not hold pointers to the `AudioManager`, so closing them
  // does not delete them.
  inner_a2dp_stream_->Close();
  inner_sco_stream_->Close();

  // Note: This must be last, it will delete |this|.
  manager_->ReleaseOutputStream(this);
}

void AAudioBluetoothOutputStream::Start(AudioSourceCallback* callback) {
  callback_ = callback;
  GetActiveInnerStream().Start(callback);
}

void AAudioBluetoothOutputStream::Stop() {
  callback_ = nullptr;
  GetActiveInnerStream().Stop();
}

void AAudioBluetoothOutputStream::Flush() {
  inner_a2dp_stream_->Flush();
  inner_sco_stream_->Flush();
}

void AAudioBluetoothOutputStream::SetVolume(double volume) {
  double volume_override = 0;
  if (manager_->HasOutputVolumeOverride(&volume_override)) {
    volume = volume_override;
  }

  inner_a2dp_stream_->SetVolume(volume);
  inner_sco_stream_->SetVolume(volume);
}

void AAudioBluetoothOutputStream::GetVolume(double* volume) {
  GetActiveInnerStream().GetVolume(volume);
}

void AAudioBluetoothOutputStream::SetMute(bool muted) {
  inner_a2dp_stream_->SetMute(muted);
  inner_sco_stream_->SetMute(muted);
}

void AAudioBluetoothOutputStream::SetUseSco(bool use_sco) {
  if (use_sco_ == use_sco) {
    // No state change; do nothing.
    return;
  }

  if (callback_) {
    GetActiveInnerStream().Stop();
  }
  use_sco_ = use_sco;  // Switch the active inner stream
  if (callback_) {
    GetActiveInnerStream().Start(callback_);
  }
}

AAudioOutputStream& AAudioBluetoothOutputStream::GetActiveInnerStream() const {
  return use_sco_ ? *inner_sco_stream_ : *inner_a2dp_stream_;
}

}  // namespace media
