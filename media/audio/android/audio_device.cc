// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_device.h"

#include "base/check_op.h"
#include "media/audio/android/audio_device_type.h"

namespace media::android {

AudioDevice::AudioDevice(AudioDeviceId id, AudioDeviceType type)
    : id_(id), type_(type) {}

AudioDevice AudioDevice::Default() {
  return AudioDevice(AudioDeviceId::Default(), AudioDeviceType::kUnknown);
}

bool AudioDevice::IsDefault() const {
  return id_.IsDefault();
}

std::optional<AudioDevice> AudioDevice::GetAssociatedScoDevice() const {
  if (!associated_sco_device_id_.has_value()) {
    return std::nullopt;
  }
  return AudioDevice(associated_sco_device_id_.value(),
                     AudioDeviceType::kBluetoothSco);
}

void AudioDevice::SetAssociatedScoDeviceId(AudioDeviceId sco_device_id) {
  // Associated SCO device IDs are only relevant for A2DP devices.
  DCHECK_EQ(type_, AudioDeviceType::kBluetoothA2dp);

  associated_sco_device_id_ = std::move(sco_device_id);
}

}  // namespace media::android
