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
  explicit CrasDevice(struct libcras_node_info* node, DeviceType type);

  DeviceType type;
  uint64_t id;
  uint32_t dev_idx;
  bool plugged;
  bool active;
  std::string node_type;
  std::string name;
  std::string dev_name;
};

// Enumerates all devices of |type|.
std::vector<CrasDevice> CrasGetAudioDevices(DeviceType type);

// Returns if there is a keyboard mic in CRAS.
bool CrasHasKeyboardMic();

// Returns if system AEC is supported in CRAS.
int CrasGetAecSupported();

// Returns the system AEC group ID. If no group ID is specified, -1 is
// returned.
int CrasGetAecGroupId();

// Returns the default output buffer size.
int CrasGetDefaultOutputBufferSize();

}  // namespace media

#endif  // MEDIA_AUDIO_CRAS_CRAS_UTIL_H_
