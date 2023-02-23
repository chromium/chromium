// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_CRAS_CRAS_UTIL_H_
#define MEDIA_AUDIO_CRAS_CRAS_UTIL_H_

#include <cras_client.h>

#include <cstdint>
#include <string>
#include <vector>

#include "media/base/media_export.h"

namespace media {

enum class DeviceType { kInput, kOutput };

struct MEDIA_EXPORT CrasDevice {
  CrasDevice();
  explicit CrasDevice(struct libcras_node_info* node, DeviceType type);
  explicit CrasDevice(DeviceType type,
                      uint64_t id,
                      uint32_t dev_idx,
                      uint32_t max_supported_channels,
                      bool plugged,
                      bool active,
                      std::string node_type,
                      std::string name,
                      std::string dev_name);

  DeviceType type;
  uint64_t id;
  uint32_t dev_idx;
  uint32_t max_supported_channels;
  bool plugged;
  bool active;
  std::string node_type;
  std::string name;
  std::string dev_name;
};

class MEDIA_EXPORT CrasUtil {
 public:
  CrasUtil();

  virtual ~CrasUtil();

  // Enumerates all devices of |type|.
  virtual std::vector<CrasDevice> CrasGetAudioDevices(DeviceType type);

  // Returns if system AEC is supported in CRAS.
  virtual int CrasGetAecSupported();

  // Returns the system AEC group ID. If no group ID is specified, -1 is
  // returned.
  virtual int CrasGetAecGroupId();

  // Returns the default output buffer size.
  virtual int CrasGetDefaultOutputBufferSize();
};

}  // namespace media

#endif  // MEDIA_AUDIO_CRAS_CRAS_UTIL_H_
