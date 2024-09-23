// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_device_description.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "media/base/localized_strings.h"

namespace media {
const char AudioDeviceDescription::kDefaultDeviceId[] = "default";
const char AudioDeviceDescription::kCommunicationsDeviceId[] = "communications";
const char AudioDeviceDescription::kLoopbackInputDeviceId[] = "loopback";
const char AudioDeviceDescription::kLoopbackWithMuteDeviceId[] =
    "loopbackWithMute";
const char AudioDeviceDescription::kLoopbackWithoutChromeId[] =
    "loopbackWithoutChrome";

namespace {
// Sanitize names which are known to contain the user's name, such as AirPods'
// default name as recommended in
// https://w3c.github.io/mediacapture-main/#sanitize-device-labels
// See crbug.com/1163072 and crbug.com/1293761 for background information..
constexpr char kAirpodsNameSubstring[] = "AirPods";  // crbug.com/1163072

// On Windows 10, "... Hands-Free AG Audio" is a special profile with
// both microphone and speakers.  "... Stereo" is another special profile
// which supports higher quality audio. Windows 11 merges the two to avoid
// confusing the user.
// TODO(crbug.com/40255253): The strings are localized by the OS which
// should be taken into account.
constexpr char kProfileNameHandsFree[] = "Hands-Free AG Audio";
constexpr char kProfileNameStereo[] = "Stereo";

void RedactDeviceName(std::string& name) {
  std::string profile;
  if (name.find(kProfileNameHandsFree) != std::string::npos) {
    profile += std::string(" ") + kProfileNameHandsFree;
  } else if (name.find(kProfileNameStereo) != std::string::npos) {
    profile += std::string(" ") + kProfileNameStereo;
  }
  if (name.find(kAirpodsNameSubstring) != std::string::npos) {
    name = kAirpodsNameSubstring + profile;
  }
}

}  // namespace

// static
bool AudioDeviceDescription::IsDefaultDevice(const std::string& device_id) {
  return device_id.empty() ||
         device_id == AudioDeviceDescription::kDefaultDeviceId;
}

// static
bool AudioDeviceDescription::IsCommunicationsDevice(
    const std::string& device_id) {
  return device_id == AudioDeviceDescription::kCommunicationsDeviceId;
}

// static
bool AudioDeviceDescription::IsLoopbackDevice(const std::string& device_id) {
  return device_id == kLoopbackInputDeviceId ||
         device_id == kLoopbackWithMuteDeviceId ||
         device_id == kLoopbackWithoutChromeId;
}

// static
bool AudioDeviceDescription::UseSessionIdToSelectDevice(
    const base::UnguessableToken& session_id,
    const std::string& device_id) {
  return !session_id.is_empty() && device_id.empty();
}

// static
std::string AudioDeviceDescription::GetDefaultDeviceName() {
  return GetLocalizedStringUTF8(DEFAULT_AUDIO_DEVICE_NAME);
}

// static
std::string AudioDeviceDescription::GetCommunicationsDeviceName() {
#if BUILDFLAG(IS_WIN)
  return GetLocalizedStringUTF8(COMMUNICATIONS_AUDIO_DEVICE_NAME);
#elif BUILDFLAG(IS_CASTOS) || BUILDFLAG(IS_CAST_ANDROID)
  // TODO(crbug.com/1336055): Re-evaluate if this is still needed now that CMA
  // is deprecated.
  return "";
#else
  NOTREACHED();
#endif
}

// static
std::string AudioDeviceDescription::GetDefaultDeviceName(
    const std::string& real_device_name) {
  if (real_device_name.empty())
    return GetDefaultDeviceName();
  // TODO(guidou): Put the names together in a localized manner.
  // http://crbug.com/788767
  return GetDefaultDeviceName() + " - " + real_device_name;
}

// static
std::string AudioDeviceDescription::GetCommunicationsDeviceName(
    const std::string& real_device_name) {
  if (real_device_name.empty())
    return GetCommunicationsDeviceName();
  // TODO(guidou): Put the names together in a localized manner.
  // http://crbug.com/788767
  return GetCommunicationsDeviceName() + " - " + real_device_name;
}

// static
void AudioDeviceDescription::LocalizeDeviceDescriptions(
    AudioDeviceDescriptions* device_descriptions) {
  for (auto& description : *device_descriptions) {
    RedactDeviceName(description.device_name);

    if (media::AudioDeviceDescription::IsDefaultDevice(description.unique_id)) {
      description.device_name =
          media::AudioDeviceDescription::GetDefaultDeviceName(
              description.device_name);
    } else if (media::AudioDeviceDescription::IsCommunicationsDevice(
                   description.unique_id)) {
      description.device_name =
          media::AudioDeviceDescription::GetCommunicationsDeviceName(
              description.device_name);
    }
  }
}

AudioDeviceDescription::AudioDeviceDescription() = default;
AudioDeviceDescription::~AudioDeviceDescription() = default;

AudioDeviceDescription::AudioDeviceDescription(
    const AudioDeviceDescription& other) = default;
AudioDeviceDescription& AudioDeviceDescription::operator=(
    const AudioDeviceDescription& other) = default;

AudioDeviceDescription::AudioDeviceDescription(AudioDeviceDescription&& other) =
    default;
AudioDeviceDescription& AudioDeviceDescription::operator=(
    AudioDeviceDescription&& other) = default;

AudioDeviceDescription::AudioDeviceDescription(std::string device_name,
                                               std::string unique_id,
                                               std::string group_id,
                                               bool is_system_default,
                                               bool is_communications_device)
    : device_name(device_name),
      unique_id(unique_id),
      group_id(group_id),
      is_system_default(is_system_default),
      is_communications_device(is_communications_device) {}

bool AudioDeviceDescription::operator==(
    const AudioDeviceDescription& other) const {
  return device_name == other.device_name && unique_id == other.unique_id &&
         group_id == other.group_id;
}

}  // namespace media
