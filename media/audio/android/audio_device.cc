// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_device.h"

namespace media::android {

AudioDevice::AudioDevice(AudioDeviceId id, AudioDeviceType type)
    : id_(id), type_(type) {}

AudioDevice AudioDevice::Default() {
  return AudioDevice(AudioDeviceId::Default(), AudioDeviceType::kUnknown);
}

bool AudioDevice::IsDefault() const {
  return id_.IsDefault();
}

}  // namespace media::android
