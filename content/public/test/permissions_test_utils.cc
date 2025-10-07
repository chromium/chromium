// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/permissions_test_utils.h"

#include "base/test/test_future.h"
#include "base/types/optional_ref.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-forward.h"

namespace content {

void SetPermissionControllerOverride(
    PermissionController* permission_controller,
    base::optional_ref<const url::Origin> requesting_origin,
    base::optional_ref<const url::Origin> embedding_origin,
    blink::PermissionType permission,
    const blink::mojom::PermissionStatus& status) {
  base::test::TestFuture<PermissionControllerImpl::OverrideStatus> future;

  PermissionControllerImpl* permission_controller_impl =
      static_cast<PermissionControllerImpl*>(permission_controller);
  permission_controller_impl->SetPermissionOverride(
      requesting_origin, embedding_origin, permission, status,
      future.GetCallback());
  ASSERT_EQ(future.Get(),
            PermissionControllerImpl::OverrideStatus::kOverrideSet);
}

void AddNotifyListenerObserver(PermissionController* permission_controller,
                               base::RepeatingClosure callback) {
  PermissionControllerImpl* permission_controller_impl =
      static_cast<PermissionControllerImpl*>(permission_controller);
  permission_controller_impl->add_notify_listener_observer_for_tests(
      std::move(callback));
}

PermissionController::SubscriptionId SubscribeToPermissionResultChange(
    PermissionController* permission_controller,
    blink::mojom::PermissionDescriptorPtr permission_descriptor,
    RenderProcessHost* render_process_host,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool should_include_device_status,
    const base::RepeatingCallback<void(PermissionResult)>& callback) {
  PermissionControllerImpl* permission_controller_impl =
      static_cast<PermissionControllerImpl*>(permission_controller);

  return permission_controller_impl->SubscribeToPermissionResultChange(
      std::move(permission_descriptor), render_process_host, render_frame_host,
      requesting_origin, should_include_device_status, callback);
}

}  // namespace content
