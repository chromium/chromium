// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/mock_native_file_system_permission_grant.h"

namespace content {

MockNativeFileSystemPermissionGrant::MockNativeFileSystemPermissionGrant() =
    default;
MockNativeFileSystemPermissionGrant::~MockNativeFileSystemPermissionGrant() =
    default;

void MockNativeFileSystemPermissionGrant::RequestPermission(
    GlobalFrameRoutingId frame_id,
    UserActivationState user_activation_state,
    base::OnceCallback<void(PermissionRequestOutcome)> callback) {
  RequestPermission_(frame_id, user_activation_state, callback);
}

}  // namespace content
