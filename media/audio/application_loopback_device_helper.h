// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_APPLICATION_LOOPBACK_DEVICE_HELPER_H_
#define MEDIA_AUDIO_APPLICATION_LOOPBACK_DEVICE_HELPER_H_

#include <cstdint>
#include <string>

#include "media/base/media_export.h"

namespace media {

std::string MEDIA_EXPORT
CreateApplicationLoopbackDeviceId(const uint32_t application_id);

uint32_t MEDIA_EXPORT
GetApplicationIdFromApplicationLoopbackDeviceId(std::string_view device_id);

}  // namespace media

#endif  // MEDIA_AUDIO_APPLICATION_LOOPBACK_DEVICE_HELPER_H_
