// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_device_id.h"

#include <cstdint>
#include <optional>
#include <string_view>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/audio/audio_device_description.h"

namespace media::android {

std::optional<AudioDeviceId> AudioDeviceId::NonDefault(int id) {
  if (id == AAUDIO_UNSPECIFIED) {
    return std::nullopt;
  }
  return AudioDeviceId(id);
}

std::optional<AudioDeviceId> AudioDeviceId::Parse(
    const std::string_view device_id_string) {
  if (AudioDeviceDescription::IsDefaultDevice(device_id_string)) {
    return AudioDeviceId::Default();
  }

  int32_t parsed_number;
  bool success = base::StringToInt(device_id_string, &parsed_number);
  if (!success) {
    DLOG(ERROR) << "Failed to parse device_id_string as number: "
                << device_id_string;
    return std::nullopt;
  }

  // An Android device ID string containing the numeric value of
  // `AAUDIO_UNSPECIFIED` is invalid, as this value is reserved for the
  // "default" device case, for which a string matching
  // `AudioDeviceDescription::IsDefaultDevice` should always be used.
  if (parsed_number == AAUDIO_UNSPECIFIED) {
    DLOG(ERROR) << "device_id_string unexpectedly contained the numeric value "
                   "reserved for the default device";
    return std::nullopt;
  }

  return AudioDeviceId(parsed_number);
}

bool AudioDeviceId::IsDefault() const {
  return id_ == AAUDIO_UNSPECIFIED;
}

int32_t AudioDeviceId::ToAAudioDeviceId() const {
  return id_;
}

}  // namespace media::android
