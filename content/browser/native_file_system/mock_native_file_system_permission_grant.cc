// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/mock_native_file_system_permission_grant.h"

namespace content {

MockNativeFileSystemPermissionGrant::MockNativeFileSystemPermissionGrant() =
    default;
MockNativeFileSystemPermissionGrant::~MockNativeFileSystemPermissionGrant() =
    default;

void MockNativeFileSystemPermissionGrant::RequestPermission(
    int process_id,
    int frame_id,
    base::OnceCallback<void(PermissionRequestOutcome)> callback) {
  RequestPermission_(process_id, frame_id, callback);
}

}  // namespace content
