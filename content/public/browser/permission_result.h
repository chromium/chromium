// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PERMISSION_RESULT_H_
#define CONTENT_PUBLIC_BROWSER_PERMISSION_RESULT_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace content {

using PermissionStatus = blink::mojom::PermissionStatus;

// Identifies the source or reason for a permission status being returned.
enum class PermissionStatusSource {
  // The reason for the status is not specified.
  UNSPECIFIED,

  // The status is the result of being blocked by the permissions kill switch.
  KILL_SWITCH,

  // The status is the result of being blocked due to the user dismissing a
  // permission prompt multiple times.
  MULTIPLE_DISMISSALS,

  // The status is the result of being blocked due to the user ignoring a
  // permission prompt multiple times.
  MULTIPLE_IGNORES,

  // This origin is insecure, thus its access to some permissions has been
  // restricted, such as camera, microphone, etc.
  INSECURE_ORIGIN,

  // The feature has been blocked in the requesting frame by permissions policy.
  FEATURE_POLICY,

  // The virtual URL and the loaded URL are for different origins. The loaded
  // URL is the one actually in the renderer, but the virtual URL is the one
  // seen by the user. This may be very confusing for a user to see in a
  // permissions request.
  VIRTUAL_URL_DIFFERENT_ORIGIN,

  // The status is the result of a permission being requested inside a fenced
  // frame. Permissions are currently always denied inside a fenced frame.
  FENCED_FRAME,

  // The status is the result of being blocked due to having recently displayed
  // the prompt to the user.
  RECENT_DISPLAY,
};

struct CONTENT_EXPORT PermissionResult {
  PermissionResult(PermissionStatus status, PermissionStatusSource source);
  ~PermissionResult();

  PermissionStatus status;
  PermissionStatusSource source;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PERMISSION_RESULT_H_
