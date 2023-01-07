// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MEDIASTREAM_MEDIA_DEVICES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MEDIASTREAM_MEDIA_DEVICES_H_

#include <string>
#include <vector>

#include "media/base/video_facing.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-shared.h"

namespace blink {

struct BLINK_COMMON_EXPORT WebMediaDeviceInfo {
  WebMediaDeviceInfo();
  WebMediaDeviceInfo(const WebMediaDeviceInfo& other);
  WebMediaDeviceInfo(WebMediaDeviceInfo&& other);
  WebMediaDeviceInfo(
      const std::string& device_id,
      const std::string& label,
      const std::string& group_id,
      const media::VideoCaptureControlSupport& video_control_support =
          media::VideoCaptureControlSupport(),
      blink::mojom::FacingMode video_facing = blink::mojom::FacingMode::NONE);
  explicit WebMediaDeviceInfo(
      const media::VideoCaptureDeviceDescriptor& descriptor);
  ~WebMediaDeviceInfo();
  WebMediaDeviceInfo& operator=(const WebMediaDeviceInfo& other);
  WebMediaDeviceInfo& operator=(WebMediaDeviceInfo&& other);

  std::string device_id;
  std::string label;
  std::string group_id;
  media::VideoCaptureControlSupport video_control_support;
  blink::mojom::FacingMode video_facing = blink::mojom::FacingMode::NONE;
};

using WebMediaDeviceInfoArray = std::vector<WebMediaDeviceInfo>;

BLINK_COMMON_EXPORT bool operator==(const WebMediaDeviceInfo& first,
                                    const WebMediaDeviceInfo& second);

inline bool IsValidMediaDeviceType(mojom::MediaDeviceType type) {
  return static_cast<size_t>(type) >= 0 &&
         static_cast<size_t>(type) <
             static_cast<size_t>(
                 mojom::MediaDeviceType::NUM_MEDIA_DEVICE_TYPES);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MEDIASTREAM_MEDIA_DEVICES_H_
