// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/fixed_file_system_access_permission_grant.h"

namespace content {

FixedFileSystemAccessPermissionGrant::FixedFileSystemAccessPermissionGrant(
    PermissionStatus status,
    PathInfo path_info)
    : status_(status), path_info_(std::move(path_info)) {}

FixedFileSystemAccessPermissionGrant::~FixedFileSystemAccessPermissionGrant() =
    default;

FixedFileSystemAccessPermissionGrant::PermissionStatus
FixedFileSystemAccessPermissionGrant::GetStatus() {
  return status_;
}

base::FilePath FixedFileSystemAccessPermissionGrant::GetPath() {
  return path_info_.path;
}

std::string FixedFileSystemAccessPermissionGrant::GetDisplayName() {
  return path_info_.display_name;
}

void FixedFileSystemAccessPermissionGrant::RequestPermission(
    GlobalRenderFrameHostId frame_id,
    UserActivationState user_activation_state,
    base::OnceCallback<void(PermissionRequestOutcome)> callback) {
  std::move(callback).Run(PermissionRequestOutcome::kRequestAborted);
}

}  // namespace content
