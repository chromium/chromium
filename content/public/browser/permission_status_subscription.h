// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PERMISSION_STATUS_SUBSCRIPTION_H_
#define CONTENT_PUBLIC_BROWSER_PERMISSION_STATUS_SUBSCRIPTION_H_

#include "content/common/content_export.h"
#include "content/public/browser/permission_result.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"

namespace content {

// The following structure defines permission status change events observed by
// frames.
struct CONTENT_EXPORT PermissionStatusSubscription {
  PermissionStatusSubscription();
  ~PermissionStatusSubscription();

  blink::PermissionType permission;
  GURL requesting_origin;
  // According to a permission delegation policy, when a cross-origin iframe
  // uses permissions, the requesting origin could be overridden by a
  // PermissionControllerDelegate.
  GURL requesting_origin_delegation;
  GURL embedding_origin;
  int render_frame_id = -1;
  int render_process_id = -1;
  bool should_include_device_status;

  base::RepeatingCallback<void(PermissionStatus, bool)> callback;
  std::optional<PermissionResult> permission_result;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PERMISSION_STATUS_SUBSCRIPTION_H_
