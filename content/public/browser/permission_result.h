// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PERMISSION_RESULT_H_
#define CONTENT_PUBLIC_BROWSER_PERMISSION_RESULT_H_

#include "components/content_settings/core/common/content_settings.h"
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

  // The status is the result of a permission being temporarily overridden by an
  // actor operating on the tab.
  ACTOR_OVERRIDE,

  // The status is the result of a permission being granted based on a
  // heuristic.
  HEURISTIC_GRANT,

  // The status is the result of app level settings (Chrome does not have
  // permission at the app level and cannot acquire it).
  APP_LEVEL_SETTINGS,
};

struct CONTENT_EXPORT PermissionResult {
  PermissionResult();
  explicit PermissionResult(PermissionStatus permission_status,
                            PermissionStatusSource permission_status_source =
                                PermissionStatusSource::UNSPECIFIED,
                            std::optional<PermissionSetting>
                                retrieved_permission_data = std::nullopt);
  PermissionResult(const PermissionResult& other);
  PermissionResult& operator=(PermissionResult& other);
  PermissionResult(PermissionResult&&);
  PermissionResult& operator=(PermissionResult&&);
  ~PermissionResult();

  bool operator==(const PermissionResult& rhs) const;

  PermissionStatus status;
  PermissionStatusSource source;

  // Holds the fully coalesced (i.e. combined persisted & ephemeral state)
  // permission state that determined the `PermissionResult`. This is used to
  // support permissions with options. It's possible that a request cannot be
  // satisfied as is, because an option has been denied, but the request can be
  // downgraded. To determine what to do, the permission implementation can use
  // this field to inspect the permission state.
  std::optional<PermissionSetting> retrieved_permission_setting;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PERMISSION_RESULT_H_
