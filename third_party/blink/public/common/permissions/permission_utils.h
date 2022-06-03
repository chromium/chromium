// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_PERMISSION_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_PERMISSION_UTILS_H_

#include <string>

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-shared.h"

namespace blink {

// Converts a permission string ("granted", "denied", "prompt") into a
// PermissionStatus.
BLINK_COMMON_EXPORT mojom::PermissionStatus ToPermissionStatus(
    const std::string& status);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_PERMISSION_UTILS_H_
