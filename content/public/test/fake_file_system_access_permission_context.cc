// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_file_system_access_permission_context.h"

#include "base/files/file_path.h"
#include "content/browser/file_system_access/fixed_file_system_access_permission_grant.h"

namespace content {

FakeFileSystemAccessPermissionContext::FakeFileSystemAccessPermissionContext() =
    default;

FakeFileSystemAccessPermissionContext::
    ~FakeFileSystemAccessPermissionContext() = default;

scoped_refptr<FileSystemAccessPermissionGrant>
FakeFileSystemAccessPermissionContext::GetReadPermissionGrant(
    const url::Origin& origin,
    const base::FilePath& path,
    HandleType handle_type,
    UserAction user_action) {
  return base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
      FileSystemAccessPermissionGrant::PermissionStatus::GRANTED, path);
}

scoped_refptr<FileSystemAccessPermissionGrant>
FakeFileSystemAccessPermissionContext::GetWritePermissionGrant(
    const url::Origin& origin,
    const base::FilePath& path,
    HandleType handle_type,
    UserAction user_action) {
  return base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
      FileSystemAccessPermissionGrant::PermissionStatus::GRANTED, path);
}

void FakeFileSystemAccessPermissionContext::ConfirmSensitiveEntryAccess(
    const url::Origin& origin,
    PathType path_type,
    const base::FilePath& path,
    HandleType handle_type,
    UserAction user_action,
    GlobalRenderFrameHostId frame_id,
    base::OnceCallback<void(SensitiveEntryResult)> callback) {
  std::move(callback).Run(SensitiveEntryResult::kAllowed);
}

void FakeFileSystemAccessPermissionContext::PerformAfterWriteChecks(
    std::unique_ptr<FileSystemAccessWriteItem> item,
    GlobalRenderFrameHostId frame_id,
    base::OnceCallback<void(AfterWriteCheckResult)> callback) {
  std::move(callback).Run(AfterWriteCheckResult::kAllow);
}

bool FakeFileSystemAccessPermissionContext::CanObtainReadPermission(
    const url::Origin& origin) {
  return true;
}
bool FakeFileSystemAccessPermissionContext::CanObtainWritePermission(
    const url::Origin& origin) {
  return true;
}

void FakeFileSystemAccessPermissionContext::SetLastPickedDirectory(
    const url::Origin& origin,
    const std::string& id,
    const base::FilePath& path,
    const PathType type) {
  PathInfo info;
  info.path = path;
  info.type = type;
  id_pathinfo_map_.insert({id, info});
}

FakeFileSystemAccessPermissionContext::PathInfo
FakeFileSystemAccessPermissionContext::GetLastPickedDirectory(
    const url::Origin& origin,
    const std::string& id) {
  return id_pathinfo_map_.find(id) != id_pathinfo_map_.end()
             ? id_pathinfo_map_[id]
             : PathInfo();
}

void FakeFileSystemAccessPermissionContext::SetWellKnownDirectoryPath(
    blink::mojom::WellKnownDirectory directory,
    base::FilePath path) {
  well_known_directory_map_.insert({directory, path});
}

base::FilePath FakeFileSystemAccessPermissionContext::GetWellKnownDirectoryPath(
    blink::mojom::WellKnownDirectory directory,
    const url::Origin& origin) {
  return well_known_directory_map_.find(directory) !=
                 well_known_directory_map_.end()
             ? well_known_directory_map_[directory]
             : base::FilePath();
}

std::u16string FakeFileSystemAccessPermissionContext::GetPickerTitle(
    const blink::mojom::FilePickerOptionsPtr& options) {
  return kPickerTitle;
}

void FakeFileSystemAccessPermissionContext::NotifyEntryMoved(
    const url::Origin& origin,
    const base::FilePath& old_path,
    const base::FilePath& new_path) {}

}  // namespace content
