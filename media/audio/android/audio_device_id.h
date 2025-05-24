// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ANDROID_AUDIO_DEVICE_ID_H_
#define MEDIA_AUDIO_ANDROID_AUDIO_DEVICE_ID_H_

#include <compare>
#include <optional>
#include <string_view>

#include "media/base/media_export.h"

namespace media::android {

// Wrapper around Android audio device IDs which handles the virtual "default"
// device and string conversion.
class MEDIA_EXPORT AudioDeviceId {
 public:
  static AudioDeviceId Default();

  // Creates an `AudioDeviceId` representing the ID of a non-default device.
  // Returns `std::nullopt` if the default device ID is provided.
  static std::optional<AudioDeviceId> NonDefault(int id);

  // Parses a string representation of a device ID to an `AudioDeviceId`.
  // Returns `std::nullopt` if parsing fails.
  static std::optional<AudioDeviceId> Parse(std::string_view device_id_string);

  bool IsDefault() const;

  // Converts the device ID to a value compatible with
  // `AAudioStreamBuilder_setDeviceId`.
  int32_t ToAAudioDeviceId() const;

  std::strong_ordering operator<=>(const AudioDeviceId& other) const = default;

 private:
  explicit AudioDeviceId(int32_t id);

  // Numeric ID compatible with AAudio and Java Android APIs, or
  // `AAUDIO_UNSPECIFIED` in the case of the "default" device.
  int32_t id_;
};

}  // namespace media::android

#endif  // MEDIA_AUDIO_ANDROID_AUDIO_DEVICE_ID_H_
