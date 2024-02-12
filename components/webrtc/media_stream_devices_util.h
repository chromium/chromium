// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBRTC_MEDIA_STREAM_DEVICES_UTIL_H_
#define COMPONENTS_WEBRTC_MEDIA_STREAM_DEVICES_UTIL_H_

#include "third_party/blink/public/common/mediastream/media_stream_request.h"

#include <string>
#include <vector>

namespace webrtc {

// Returns a version of `devices` containing only devices with ids found in
// `eligible_device_ids`.
blink::MediaStreamDevices FilterMediaDevices(
    blink::MediaStreamDevices devices,
    const std::vector<std::string>& eligible_device_ids);

}  // namespace webrtc
#endif  // COMPONENTS_WEBRTC_MEDIA_STREAM_DEVICES_UTIL_H_
