// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_device_name.h"

#include <utility>

#include "media/audio/audio_device_description.h"

namespace media {

AudioDeviceName::AudioDeviceName() = default;

AudioDeviceName::AudioDeviceName(std::string device_name, std::string unique_id)
    : device_name(std::move(device_name)), unique_id(std::move(unique_id)) {}

// static
AudioDeviceName AudioDeviceName::CreateDefault() {
  return AudioDeviceName(std::string(),
                         AudioDeviceDescription::kDefaultDeviceId);
}

// static
AudioDeviceName AudioDeviceName::CreateCommunications() {
  return AudioDeviceName(std::string(),
                         AudioDeviceDescription::kCommunicationsDeviceId);
}

}  // namespace media
