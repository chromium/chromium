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
  // Virtual for testing.
  virtual std::vector<CrasDevice> CrasGetAudioDevices(DeviceType type);

  // Returns if system AEC is supported in CRAS.
  // Virtual for testing.
  virtual int CrasGetAecSupported();

  // Returns if system AGC is supported in CRAS.
  // Virtual for testing.
  virtual int CrasGetAgcSupported();

  // Returns if system NS is supported in CRAS.
  // Virtual for testing.
  virtual int CrasGetNsSupported();

  // Returns if system Voice Isolation is supported in CRAS.
  // Virtual for testing.
  virtual int CrasGetVoiceIsolationSupported();

  // Returns the system AEC group ID. If no group ID is specified, -1 is
  // returned.
  // Virtual for testing.
  virtual int CrasGetAecGroupId();

  // Returns the default output buffer size.
  // Virtual for testing.
  virtual int CrasGetDefaultOutputBufferSize();

 private:
  // These booleans are in int type since libcras functions take these members
  // as pointers.
  int aec_supported_ = false;
  int agc_supported_ = false;
  int ns_supported_ = false;
  int voice_isolation_supported_ = false;

  int aec_group_id_ = -1;
  int default_output_buffer_size_ = 0;
  bool cras_effects_cached_ = false;

  // Caches constant effect config from CRAS.
  bool CacheEffects();
};

}  // namespace media

#endif  // MEDIA_AUDIO_CRAS_CRAS_UTIL_H_
