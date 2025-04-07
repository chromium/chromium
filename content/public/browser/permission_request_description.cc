// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/permission_request_description.h"

namespace content {

PermissionRequestDescription::PermissionRequestDescription(
    std::vector<blink::mojom::PermissionDescriptorPtr> permissions,
    bool user_gesture,
    const GURL& requesting_origin,
    bool embedded_permission_element_initiated,
    const std::optional<gfx::Rect>& anchor_element_position)
    : permissions(std::move(permissions)),
      user_gesture(user_gesture),
      requesting_origin(requesting_origin),
      embedded_permission_element_initiated(
          embedded_permission_element_initiated),
      anchor_element_position(anchor_element_position) {}

PermissionRequestDescription::PermissionRequestDescription(
    blink::mojom::PermissionDescriptorPtr permission,
    bool user_gesture,
    const GURL& requesting_origin,
    bool embedded_permission_element_initiated,
    const std::optional<gfx::Rect>& anchor_element_position)
    : user_gesture(user_gesture),
      requesting_origin(requesting_origin),
      embedded_permission_element_initiated(
          embedded_permission_element_initiated),
      anchor_element_position(anchor_element_position) {
  permissions.push_back(std::move(permission));
}

PermissionRequestDescription::PermissionRequestDescription(
    const PermissionRequestDescription& other) {
  for (const auto& permission : other.permissions) {
    permissions.push_back(permission.Clone());
  }
  user_gesture = other.user_gesture;
  requesting_origin = other.requesting_origin;
  embedded_permission_element_initiated =
      other.embedded_permission_element_initiated;
  anchor_element_position = other.anchor_element_position;
  requested_audio_capture_device_ids = other.requested_audio_capture_device_ids;
  requested_video_capture_device_ids = other.requested_video_capture_device_ids;
}

PermissionRequestDescription& PermissionRequestDescription::operator=(
    PermissionRequestDescription&&) = default;
PermissionRequestDescription::PermissionRequestDescription(
    PermissionRequestDescription&&) = default;
PermissionRequestDescription::~PermissionRequestDescription() = default;

bool PermissionRequestDescription::operator==(
    const PermissionRequestDescription& other) const {
  return user_gesture == other.user_gesture &&
         embedded_permission_element_initiated ==
             other.embedded_permission_element_initiated &&
         requesting_origin == other.requesting_origin &&
         permissions == other.permissions &&
         anchor_element_position == other.anchor_element_position;
}

}  // namespace content
