// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/permission_request_description.h"

namespace content {

PermissionRequestDescription::PermissionRequestDescription(
    std::vector<blink::mojom::PermissionDescriptorPtr> permissions,
    bool user_gesture,
    const GURL& requesting_origin)
    : permissions(std::move(permissions)),
      user_gesture(user_gesture),
      requesting_origin(requesting_origin) {}

PermissionRequestDescription::PermissionRequestDescription(
    blink::mojom::PermissionDescriptorPtr permission,
    bool user_gesture,
    const GURL& requesting_origin)
    : user_gesture(user_gesture), requesting_origin(requesting_origin) {
  permissions.push_back(std::move(permission));
}

PermissionRequestDescription::PermissionRequestDescription(
    std::vector<blink::mojom::PermissionDescriptorPtr> permissions,
    blink::mojom::EmbeddedPermissionRequestDescriptorPtr descriptor)
    : permissions(std::move(permissions)),
      user_gesture(true),
      embedded_permission_request_descriptor(std::move(descriptor)) {
  CHECK(embedded_permission_request_descriptor);
}

PermissionRequestDescription::PermissionRequestDescription(
    const PermissionRequestDescription& other) {
  for (const auto& permission : other.permissions) {
    permissions.push_back(permission.Clone());
  }
  user_gesture = other.user_gesture;
  requesting_origin = other.requesting_origin;
  if (other.embedded_permission_request_descriptor) {
    embedded_permission_request_descriptor =
        other.embedded_permission_request_descriptor.Clone();
  }
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
         requesting_origin == other.requesting_origin &&
         permissions == other.permissions &&
         embedded_permission_request_descriptor ==
             other.embedded_permission_request_descriptor;
}

}  // namespace content
