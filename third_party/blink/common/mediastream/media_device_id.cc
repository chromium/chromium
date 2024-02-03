// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/mediastream/media_device_id.h"

#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "media/audio/audio_device_description.h"

namespace blink {

bool IsValidMediaDeviceId(const std::string& device_id) {
  constexpr size_t hash_size = 64;  // 32 bytes * 2 char/byte hex encoding
  if (media::AudioDeviceDescription::IsDefaultDevice(device_id) ||
      device_id == media::AudioDeviceDescription::kCommunicationsDeviceId) {
    return true;
  }

  if (device_id.length() != hash_size) {
    return false;
  }

  return base::ranges::all_of(device_id, [](const char& c) {
    return base::IsAsciiLower(c) || base::IsAsciiDigit(c);
  });
}

}  // namespace blink
