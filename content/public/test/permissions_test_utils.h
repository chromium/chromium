// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_PERMISSIONS_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_PERMISSIONS_TEST_UTILS_H_

#include <optional>

#include "content/public/browser/permission_controller.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace blink {
enum class PermissionType;
}

namespace url {
class Origin;
}

namespace content {

// A helper method that gives access to
// content/browser/PermissionControllerImpl::SetOverrideForDevTools to //content
// embedders in tests.
void SetPermissionControllerOverrideForDevTools(
    PermissionController* permission_controller,
    const std::optional<url::Origin>& origin,
    blink::PermissionType permission,
    const blink::mojom::PermissionStatus& status);

// A helper method that gives access to
// content/browser/PermissionControllerImpl::AddNotifyListenerObserver to
// //content embedders in tests.
void AddNotifyListenerObserver(PermissionController* permission_controller,
                               base::RepeatingClosure callback);

// A helper method that gives access to
// content/browser/PermissionControllerImpl::SubscribeToPermissionStatusChange
// to
// //content embedders in tests.
//
// PermissionController::UnsubscribeFromPermissionStatusChange can be used to
// unsubscribe.
PermissionController::SubscriptionId SubscribeToPermissionStatusChange(
    PermissionController* permission_controller,
    blink::PermissionType permission,
    RenderProcessHost* render_process_host,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool should_include_device_status,
    const base::RepeatingCallback<void(PermissionStatus)>& callback);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_PERMISSIONS_TEST_UTILS_H_
