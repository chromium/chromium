// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_DEVICE_NAME_H_
#define MEDIA_AUDIO_AUDIO_DEVICE_NAME_H_

#include <list>
#include <string>
#include "media/base/media_export.h"

namespace media {

struct MEDIA_EXPORT AudioDeviceName {
  AudioDeviceName();
  AudioDeviceName(std::string device_name, std::string unique_id);

  // Creates default device representation.
  // Shouldn't be used in the audio service, since the audio service doesn't
  // have access to localized device names.
  static AudioDeviceName CreateDefault();

  // Creates communications device representation.
  // Shouldn't be used in the audio service, since the audio service doesn't
  // have access to localized device names.
  static AudioDeviceName CreateCommunications();

  std::string device_name;  // Friendly name of the device.
  std::string unique_id;    // Unique identifier for the device.
};

typedef std::list<AudioDeviceName> AudioDeviceNames;

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_DEVICE_NAME_H_
