// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_PERMISSIONS_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_PERMISSIONS_TEST_UTILS_H_

#include <optional>
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace blink {
enum class PermissionType;
}

namespace url {
class Origin;
}

namespace content {

class PermissionController;

void SetPermissionControllerOverrideForDevTools(
    PermissionController* permission_controller,
    const std::optional<url::Origin>& origin,
    blink::PermissionType permission,
    const blink::mojom::PermissionStatus& status);

void AddNotifyListenerObserver(PermissionController* permission_controller,
                               base::RepeatingClosure callback);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_PERMISSIONS_TEST_UTILS_H_
