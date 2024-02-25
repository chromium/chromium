// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-shared.h"

namespace blink {

WebMediaDeviceInfo::WebMediaDeviceInfo() = default;

WebMediaDeviceInfo::WebMediaDeviceInfo(const WebMediaDeviceInfo& other) =
    default;

WebMediaDeviceInfo::WebMediaDeviceInfo(WebMediaDeviceInfo&& other) = default;

WebMediaDeviceInfo::WebMediaDeviceInfo(
    const std::string& device_id,
    const std::string& label,
    const std::string& group_id,
    const media::VideoCaptureControlSupport& video_control_support,
    blink::mojom::FacingMode video_facing,
    std::optional<media::CameraAvailability> availability)
    : device_id(device_id),
      label(label),
      group_id(group_id),
      video_control_support(video_control_support),
      video_facing(video_facing),
      availability(std::move(availability)) {}

WebMediaDeviceInfo::WebMediaDeviceInfo(
    const media::VideoCaptureDeviceDescriptor& descriptor)
    : device_id(descriptor.device_id),
      label(descriptor.GetNameAndModel()),
      video_control_support(descriptor.control_support()),
      video_facing(static_cast<blink::mojom::FacingMode>(descriptor.facing)),
      availability(descriptor.availability) {}

WebMediaDeviceInfo::~WebMediaDeviceInfo() = default;

WebMediaDeviceInfo& WebMediaDeviceInfo::operator=(
    const WebMediaDeviceInfo& other) = default;

WebMediaDeviceInfo& WebMediaDeviceInfo::operator=(WebMediaDeviceInfo&& other) =
    default;

bool operator==(const WebMediaDeviceInfo& first,
                const WebMediaDeviceInfo& second) {
  // Do not use the |group_id| and |video_facing| fields for equality comparison
  // since they are currently not fully supported by the video-capture layer.
  // The modification of those fields by heuristics in upper layers does not
  // result in a different device.
  return first.device_id == second.device_id && first.label == second.label;
}

}  // namespace blink
