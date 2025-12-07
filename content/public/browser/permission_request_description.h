// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PERMISSION_REQUEST_DESCRIPTION_H_
#define CONTENT_PUBLIC_BROWSER_PERMISSION_REQUEST_DESCRIPTION_H_

#include <vector>

#include "content/common/content_export.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace content {

// This structure represents a subset of information necessary to request
// permission from a renderer, including important contextual information.
struct CONTENT_EXPORT PermissionRequestDescription {
  explicit PermissionRequestDescription(
      std::vector<blink::mojom::PermissionDescriptorPtr> permissions,
      bool user_gesture = false,
      const GURL& requesting_origin = GURL());

  explicit PermissionRequestDescription(
      blink::mojom::PermissionDescriptorPtr permission,
      bool user_gesture = false,
      const GURL& requesting_origin = GURL());

  explicit PermissionRequestDescription(
      std::vector<blink::mojom::PermissionDescriptorPtr> permissions,
      blink::mojom::EmbeddedPermissionRequestDescriptorPtr descriptor);

  PermissionRequestDescription& operator=(const PermissionRequestDescription&) =
      delete;
  // The copy constructor is used in GMOCK-based tests which need to be able to
  // copy the object in order to create the corresponding Matcher.
  PermissionRequestDescription(const PermissionRequestDescription&);

  PermissionRequestDescription& operator=(PermissionRequestDescription&&);
  PermissionRequestDescription(PermissionRequestDescription&&);
  ~PermissionRequestDescription();

  bool operator==(const PermissionRequestDescription& other) const;

  // Defines the list of permissions we will request.
  std::vector<blink::mojom::PermissionDescriptorPtr> permissions;

  // Indicates the request is initiated by a user gesture.
  bool user_gesture;

  // The origin on whose behalf this permission request is being made.
  GURL requesting_origin;

  // If not null, this request comes from an embedded permission element,
  // and this struct holds element-specific data.
  blink::mojom::EmbeddedPermissionRequestDescriptorPtr
      embedded_permission_request_descriptor;
  std::vector<std::string> requested_audio_capture_device_ids;
  std::vector<std::string> requested_video_capture_device_ids;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PERMISSION_REQUEST_DESCRIPTION_H_
