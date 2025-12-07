// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PERMISSION_DESCRIPTOR_UTIL_H_
#define CONTENT_PUBLIC_BROWSER_PERMISSION_DESCRIPTOR_UTIL_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"

namespace content {

class PermissionDescriptorUtil {
 public:
  // Utility function that creates a default `PermissionDescriptorPtr` for the a
  // PermissionType. Used only for
  // simple ContentSetting types without options while refactorings to support
  // permission options are performed.
  // TODO(https://crbug.com/406755622): Remove mapping function
  CONTENT_EXPORT static blink::mojom::PermissionDescriptorPtr
  CreatePermissionDescriptorForPermissionType(
      blink::PermissionType permission_type);

  // Utility function that creates a vector of default
  // `PermissionDescriptorPtr`s for a vector of `PermissionType. Used only for
  // simple ContentSetting types without options while refactorings to support
  // permission options are performed.
  // TODO(https://crbug.com/406755622): Remove mapping function
  CONTENT_EXPORT static std::vector<blink::mojom::PermissionDescriptorPtr>
  CreatePermissionDescriptorForPermissionTypes(
      const std::vector<blink::PermissionType>& permission_types);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PERMISSION_DESCRIPTOR_UTIL_H_
