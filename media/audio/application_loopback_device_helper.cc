// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/application_loopback_device_helper.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "media/audio/audio_device_description.h"

namespace media {

std::string MEDIA_EXPORT
CreateApplicationLoopbackDeviceId(const uint32_t application_id) {
  std::string id = AudioDeviceDescription::kApplicationLoopbackDeviceId;
  id.append(":");
  id.append(base::NumberToString(application_id));
  return id;
}

uint32_t MEDIA_EXPORT
GetApplicationIdFromApplicationLoopbackDeviceId(std::string_view device_id) {
  CHECK(AudioDeviceDescription::IsApplicationLoopbackDevice(device_id));

  size_t colon_pos = device_id.find(':');
  CHECK(colon_pos != std::string::npos);

  uint32_t application_id;
  bool valid =
      base::StringToUint(device_id.substr(colon_pos + 1), &application_id);
  CHECK(valid);

  return application_id;
}

}  // namespace media
