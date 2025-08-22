// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_device.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "media/audio/android/audio_device_id.h"
#include "media/audio/android/audio_device_type.h"

namespace media::android {

AudioDevice::AudioDevice(AudioDeviceId id,
                         AudioDeviceType type,
                         std::optional<std::string> name,
                         std::optional<std::vector<int>> sample_rates)
    : id_(id),
      type_(type),
      name_(name),
      sample_rates_(std::move(sample_rates)) {}

AudioDevice::AudioDevice(const AudioDevice& other)
    : id_(other.id_),
      type_(other.type_),
      name_(other.name_),
      sample_rates_(other.sample_rates_),
      associated_sco_device_(
          other.associated_sco_device_
              ? std::make_unique<AudioDevice>(*other.associated_sco_device_)
              : nullptr) {}

AudioDevice& AudioDevice::operator=(const AudioDevice& other) {
  id_ = other.id_;
  type_ = other.type_;
  name_ = other.name_;
  sample_rates_ = other.sample_rates_;
  associated_sco_device_ =
      other.associated_sco_device_
          ? std::make_unique<AudioDevice>(*other.associated_sco_device_)
          : nullptr;
  return *this;
}

AudioDevice::AudioDevice(AudioDevice&& other) = default;

AudioDevice& AudioDevice::operator=(AudioDevice&& other) = default;

AudioDevice::~AudioDevice() = default;

AudioDevice AudioDevice::Default() {
  return AudioDevice(AudioDeviceId::Default(), AudioDeviceType::kUnknown,
                     /*name=*/std::nullopt,
                     /*sample_rates=*/std::nullopt  // Unknown sample rates
  );
}

bool AudioDevice::IsDefault() const {
  return id_.IsDefault();
}

std::optional<AudioDevice> AudioDevice::GetAssociatedScoDevice() const {
  if (!associated_sco_device_) {
    return std::nullopt;
  }
  return *associated_sco_device_;
}

void AudioDevice::SetAssociatedScoDevice(
    std::unique_ptr<const AudioDevice> sco_device) {
  // Associations are only relevant between A2DP and SCO devices.
  DCHECK_EQ(type_, AudioDeviceType::kBluetoothA2dp);
  DCHECK_EQ(sco_device->type_, AudioDeviceType::kBluetoothSco);

  associated_sco_device_ = std::move(sco_device);
}

}  // namespace media::android
