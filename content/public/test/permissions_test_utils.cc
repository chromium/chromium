// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/permissions_test_utils.h"

#include "content/browser/permissions/permission_controller_impl.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace content {

void SetPermissionControllerOverrideForDevTools(
    PermissionController* permission_controller,
    const std::optional<url::Origin>& origin,
    blink::PermissionType permission,
    const blink::mojom::PermissionStatus& status) {
  PermissionControllerImpl* permission_controller_impl =
      static_cast<PermissionControllerImpl*>(permission_controller);
  permission_controller_impl->SetOverrideForDevTools(origin, permission,
                                                     status);
}

void AddNotifyListenerObserver(PermissionController* permission_controller,
                               base::RepeatingClosure callback) {
  PermissionControllerImpl* permission_controller_impl =
      static_cast<PermissionControllerImpl*>(permission_controller);
  permission_controller_impl->add_notify_listener_observer_for_tests(
      std::move(callback));
}

PermissionController::SubscriptionId SubscribeToPermissionStatusChange(
    PermissionController* permission_controller,
    PermissionType permission,
    RenderProcessHost* render_process_host,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool should_include_device_status,
    const base::RepeatingCallback<void(PermissionStatus)>& callback) {
  PermissionControllerImpl* permission_controller_impl =
      static_cast<PermissionControllerImpl*>(permission_controller);

  return permission_controller_impl->SubscribeToPermissionStatusChange(
      permission, render_process_host, render_frame_host, requesting_origin,
      should_include_device_status, callback);
}

}  // namespace content
