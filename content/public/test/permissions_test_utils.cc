// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/permissions_test_utils.h"

#include "content/browser/permissions/permission_controller_impl.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace content {

void SetPermissionControllerOverrideForDevTools(
    PermissionController* permission_controller,
    const absl::optional<url::Origin>& origin,
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

}  // namespace content
