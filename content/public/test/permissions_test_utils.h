// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_PERMISSIONS_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_PERMISSIONS_TEST_UTILS_H_

#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace url {
class Origin;
}

namespace content {

class PermissionController;
enum class PermissionType;

void SetPermissionControllerOverrideForDevTools(
    PermissionController* permission_controller,
    const absl::optional<url::Origin>& origin,
    PermissionType permission,
    const blink::mojom::PermissionStatus& status);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_PERMISSIONS_TEST_UTILS_H_
