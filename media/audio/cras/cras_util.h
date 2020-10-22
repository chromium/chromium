// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_CRAS_CRAS_UTIL_H_
#define MEDIA_AUDIO_CRAS_CRAS_UTIL_H_

#include <cras_client.h>

#include <cstdint>
#include <string>
#include <vector>

namespace media {

enum class DeviceType { kInput, kOutput };

struct CrasDevice {
  CrasDevice();
  explicit CrasDevice(const cras_ionode_info* node,
                      const cras_iodev_info* dev,
                      DeviceType type);
  DeviceType type;
  uint64_t id;
  std::string name;
};

// Enumerates all devices of |type|.
std::vector<CrasDevice> CrasGetAudioDevices(DeviceType type);

}  // namespace media

#endif  // MEDIA_AUDIO_CRAS_CRAS_UTIL_H_
