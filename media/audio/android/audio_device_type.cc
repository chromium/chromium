// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_device_type.h"

namespace media::android {

std::optional<AudioDeviceType> IntToAudioDeviceType(int value) {
  constexpr int kMinValue = 0;
  constexpr int kMaxValue = static_cast<int>(AudioDeviceType::kMaxValue);
  if (value < kMinValue || value > kMaxValue) {
    return std::nullopt;
  }
  return static_cast<AudioDeviceType>(value);
}

}  // namespace media::android
