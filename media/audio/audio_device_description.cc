// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_device_description.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "media/base/localized_strings.h"

namespace media {
const char AudioDeviceDescription::kDefaultDeviceId[] = "default";
const char AudioDeviceDescription::kCommunicationsDeviceId[] = "communications";
const char AudioDeviceDescription::kLoopbackInputDeviceId[] = "loopback";
const char AudioDeviceDescription::kLoopbackWithMuteDeviceId[] =
    "loopbackWithMute";
const char AudioDeviceDescription::kLoopbackWithMuteDeviceIdCast[] =
    "loopbackWithMuteCast";
const char AudioDeviceDescription::kLoopbackWithoutChromeId[] =
    "loopbackWithoutChrome";
const char AudioDeviceDescription::kLoopbackAllDevicesId[] =
    "loopbackAllDevices";
const char AudioDeviceDescription::kApplicationLoopbackDeviceId[] =
    "applicationLoopback";
const char AudioDeviceDescription::kRestrictOwnAudioBrowserLoopbackDeviceId[] =
    "restrictOwnAudioBrowserLoopback";

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
constexpr char kProfileNameHandsFreeShort[] = "Hands-Free";
constexpr char kProfileNameStereo[] = "Stereo";

// Windows native device names often include the form factor and connection
// type.
constexpr char kHeadphonesPrefix[] = "Headphones";
constexpr char kHeadsetPrefix[] = "Headset";
constexpr char kBluetoothSuffix[] = "(Bluetooth)";

void RedactDeviceName(std::string& name) {
  // The goal is to sanitize the device name to prevent PII leakage (e.g.,
  // "Henrik's AirPods") while preserving useful native OS formatting such as
  // audio profiles, form factors, and connection types.

  std::string profile;
  // Extract the audio profile. We check the longer "Hands-Free AG Audio"
  // string first before falling back to the shorter "Hands-Free" variant.
  if (name.find(kProfileNameHandsFree) != std::string::npos) {
    profile += std::string(" ") + kProfileNameHandsFree;
  } else if (name.find(kProfileNameHandsFreeShort) != std::string::npos) {
    profile += std::string(" ") + kProfileNameHandsFreeShort;
  } else if (name.find(kProfileNameStereo) != std::string::npos) {
    profile += std::string(" ") + kProfileNameStereo;
  }

  std::string form_factor;
  bool has_parentheses = false;
  // Extract Windows native form factor prefixes. These usually precede the
  // actual device name wrapped in parentheses (e.g., "Headphones (User's
  // AirPods) (Bluetooth)").
  if (name.find(kHeadphonesPrefix) != std::string::npos) {
    form_factor = std::string(kHeadphonesPrefix) + " ";
    has_parentheses = true;
  } else if (name.find(kHeadsetPrefix) != std::string::npos) {
    form_factor = std::string(kHeadsetPrefix) + " ";
    has_parentheses = true;
  }

  std::string suffix;
  // Extract connection type suffixes (e.g., "(Bluetooth)").
  if (name.find(kBluetoothSuffix) != std::string::npos) {
    suffix += std::string(" ") + kBluetoothSuffix;
  }

  // If the device is an AirPods device, rebuild the string using only the
  // safe, extracted components. All other text (including the user's name)
  // is discarded to protect privacy.
  if (name.find(kAirpodsNameSubstring) != std::string::npos) {
    std::string base_name = kAirpodsNameSubstring + profile;
    if (has_parentheses) {
      // Reconstruct the Windows format, e.g.: "Headphones (AirPods)
      // (Bluetooth)"
      name = form_factor + "(" + base_name + ")" + suffix;
    } else {
      // Fallback if no form factor parentheses were detected.
      name = base_name + suffix;
    }
  }
}

}  // namespace

// static
bool AudioDeviceDescription::IsDefaultDevice(std::string_view device_id) {
  return device_id.empty() ||
         device_id == AudioDeviceDescription::kDefaultDeviceId;
}

// static
bool AudioDeviceDescription::IsCommunicationsDevice(
    std::string_view device_id) {
  return device_id == AudioDeviceDescription::kCommunicationsDeviceId;
}

// static
bool AudioDeviceDescription::IsLoopbackDevice(std::string_view device_id) {
  return device_id == kLoopbackInputDeviceId ||
         device_id == kLoopbackWithMuteDeviceId ||
         device_id == kLoopbackWithMuteDeviceIdCast ||
         device_id == kLoopbackWithoutChromeId ||
         device_id == kLoopbackAllDevicesId ||
         IsApplicationLoopbackDevice(device_id);
}

// static
bool AudioDeviceDescription::IsApplicationLoopbackDevice(
    std::string_view device_id) {
  return base::StartsWith(device_id, kApplicationLoopbackDeviceId) ||
         base::StartsWith(device_id, kRestrictOwnAudioBrowserLoopbackDeviceId);
}

// static
bool AudioDeviceDescription::UseSessionIdToSelectDevice(
    const base::UnguessableToken& session_id,
    std::string_view device_id) {
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
    std::string_view real_device_name) {
  if (real_device_name.empty()) {
    return GetDefaultDeviceName();
  }
  // TODO(guidou): Put the names together in a localized manner.
  // http://crbug.com/788767
  return base::StringPrintf("%s - %s", GetDefaultDeviceName(),
                            real_device_name);
}

// static
std::string AudioDeviceDescription::GetCommunicationsDeviceName(
    std::string_view real_device_name) {
  if (real_device_name.empty()) {
    return GetCommunicationsDeviceName();
  }
  // TODO(guidou): Put the names together in a localized manner.
  // http://crbug.com/788767
  return base::StringPrintf("%s - %s", GetCommunicationsDeviceName(),
                            real_device_name);
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
    : device_name(std::move(device_name)),
      unique_id(std::move(unique_id)),
      group_id(std::move(group_id)),
      is_system_default(is_system_default),
      is_communications_device(is_communications_device) {}

bool AudioDeviceDescription::operator==(
    const AudioDeviceDescription& other) const {
  return device_name == other.device_name && unique_id == other.unique_id &&
         group_id == other.group_id;
}

}  // namespace media
