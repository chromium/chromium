// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/fixed_native_file_system_permission_grant.h"

namespace content {

FixedNativeFileSystemPermissionGrant::FixedNativeFileSystemPermissionGrant(
    PermissionStatus status)
    : status_(status) {}

FixedNativeFileSystemPermissionGrant::~FixedNativeFileSystemPermissionGrant() =
    default;

FixedNativeFileSystemPermissionGrant::PermissionStatus
FixedNativeFileSystemPermissionGrant::GetStatus() {
  return status_;
}

void FixedNativeFileSystemPermissionGrant::RequestPermission(
    int process_id,
    int frame_id,
    base::OnceCallback<void(PermissionRequestOutcome)> callback) {
  std::move(callback).Run(PermissionRequestOutcome::kRequestAborted);
}

}  // namespace content
