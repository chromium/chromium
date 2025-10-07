// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_PERMISSIONS_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_PERMISSIONS_TEST_UTILS_H_

#include <optional>

#include "base/types/optional_ref.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace blink {
enum class PermissionType;
}

namespace url {
class Origin;
}

// A matcher that matches permission descriptors to their PermissionType
MATCHER_P(PermissionDescriptorToPermissionTypeMatcher, permission_type, "") {
  return ::testing::Matches(::testing::Eq(permission_type))(
      blink::PermissionDescriptorToPermissionType(arg));
}
namespace content {

// A helper method that gives access to
// content/browser/PermissionControllerImpl::SetPermissionOverride to //content
// embedders in tests.
void SetPermissionControllerOverride(
    PermissionController* permission_controller,
    base::optional_ref<const url::Origin> requesting_origin,
    base::optional_ref<const url::Origin> embedding_origin,
    blink::PermissionType permission,
    const blink::mojom::PermissionStatus& status);

// A helper method that gives access to
// content/browser/PermissionControllerImpl::AddNotifyListenerObserver to
// //content embedders in tests.
void AddNotifyListenerObserver(PermissionController* permission_controller,
                               base::RepeatingClosure callback);

// A helper method that gives access to
// content/browser/PermissionControllerImpl::SubscribeToPermissionResultChange
// to
// //content embedders in tests.
//
// PermissionController::UnsubscribeFromPermissionResultChange can be used to
// unsubscribe.
PermissionController::SubscriptionId SubscribeToPermissionResultChange(
    PermissionController* permission_controller,
    blink::mojom::PermissionDescriptorPtr permission_descriptor,
    RenderProcessHost* render_process_host,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool should_include_device_status,
    const base::RepeatingCallback<void(PermissionResult)>& callback);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_PERMISSIONS_TEST_UTILS_H_
