// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/fixed_native_file_system_permission_grant.h"

namespace content {

FixedNativeFileSystemPermissionGrant::FixedNativeFileSystemPermissionGrant(
    PermissionStatus status,
    base::FilePath path)
    : status_(status), path_(std::move(path)) {}

FixedNativeFileSystemPermissionGrant::~FixedNativeFileSystemPermissionGrant() =
    default;

FixedNativeFileSystemPermissionGrant::PermissionStatus
FixedNativeFileSystemPermissionGrant::GetStatus() {
  return status_;
}

base::FilePath FixedNativeFileSystemPermissionGrant::GetPath() {
  return path_;
}

void FixedNativeFileSystemPermissionGrant::RequestPermission(
    GlobalFrameRoutingId frame_id,
    UserActivationState user_activation_state,
    base::OnceCallback<void(PermissionRequestOutcome)> callback) {
  std::move(callback).Run(PermissionRequestOutcome::kRequestAborted);
}

}  // namespace content
