// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/permission_controller_delegate.h"

namespace content {

bool PermissionControllerDelegate::IsPermissionOverridableByDevTools(
    PermissionType permission,
    const url::Origin& origin) {
  return true;
}

}  // namespace content
