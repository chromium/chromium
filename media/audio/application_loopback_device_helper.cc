// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/application_loopback_device_helper.h"

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "media/audio/audio_device_description.h"

namespace media {

#if BUILDFLAG(IS_WIN)

namespace {

std::string BuildDeviceId(std::string_view base_id,
                          const uint32_t application_id) {
  return base::StrCat({base_id, ":", base::NumberToString(application_id)});
}

}  // namespace

std::string MEDIA_EXPORT
CreateApplicationLoopbackDeviceId(const uint32_t application_id) {
  return BuildDeviceId(AudioDeviceDescription::kApplicationLoopbackDeviceId,
                       application_id);
}

std::string MEDIA_EXPORT CreateRestrictOwnAudioBrowserLoopbackDeviceId() {
  return BuildDeviceId(
      AudioDeviceDescription::kRestrictOwnAudioBrowserLoopbackDeviceId,
      base::GetCurrentProcId());
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

#elif BUILDFLAG(IS_MAC)

namespace {

std::string BuildDeviceId(std::string_view base_id,
                          std::string_view bundle_id,
                          std::optional<pid_t> application_id) {
  std::string device_id = base::StrCat({base_id, ":", bundle_id});
  if (application_id.has_value()) {
    device_id =
        base::StrCat({device_id, ":", base::NumberToString(*application_id)});
  }
  return device_id;
}

}  // namespace

std::string MEDIA_EXPORT
CreateApplicationLoopbackDeviceId(std::string_view bundle_id,
                                  std::optional<pid_t> application_id) {
  return BuildDeviceId(AudioDeviceDescription::kApplicationLoopbackDeviceId,
                       bundle_id, application_id);
}

std::string MEDIA_EXPORT
CreateRestrictOwnAudioBrowserLoopbackDeviceId(std::string_view bundle_id,
                                              pid_t application_id) {
  return BuildDeviceId(
      AudioDeviceDescription::kRestrictOwnAudioBrowserLoopbackDeviceId,
      bundle_id, std::make_optional(application_id));
}

std::pair<std::string, std::optional<pid_t>> MEDIA_EXPORT
ParseApplicationLoopbackDeviceId(std::string_view device_id) {
  CHECK(AudioDeviceDescription::IsApplicationLoopbackDevice(device_id));
  size_t first_colon_pos = device_id.find(':');
  CHECK(first_colon_pos != std::string::npos);

  size_t second_colon_pos = device_id.find(':', first_colon_pos + 1);

  std::string bundle_id = std::string(device_id.substr(
      first_colon_pos + 1, second_colon_pos - first_colon_pos - 1));
  CHECK(!bundle_id.empty());

  std::optional<pid_t> application_id;
  if (second_colon_pos != std::string::npos) {
    std::string_view application_id_str =
        device_id.substr(second_colon_pos + 1);
    if (!application_id_str.empty()) {
      pid_t pid;
      if (base::StringToInt(application_id_str, &pid)) {
        application_id = pid;
      }
    }
  }

  return {bundle_id, application_id};
}

#endif  // BUILDFLAG(IS_WIN)

bool MEDIA_EXPORT
IsRestrictOwnAudioBrowserLoopbackDeviceId(std::string_view device_id) {
  return base::StartsWith(
      device_id,
      AudioDeviceDescription::kRestrictOwnAudioBrowserLoopbackDeviceId);
}

}  // namespace media
