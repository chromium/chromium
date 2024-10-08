// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_file_system_access_permission_context.h"

#include "base/containers/contains.h"
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
    const PathInfo& path_info,
    HandleType handle_type,
    UserAction user_action) {
  return base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
      FileSystemAccessPermissionGrant::PermissionStatus::GRANTED, path_info);
}

scoped_refptr<FileSystemAccessPermissionGrant>
FakeFileSystemAccessPermissionContext::GetWritePermissionGrant(
    const url::Origin& origin,
    const PathInfo& path_info,
    HandleType handle_type,
    UserAction user_action) {
  return base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
      FileSystemAccessPermissionGrant::PermissionStatus::GRANTED, path_info);
}

void FakeFileSystemAccessPermissionContext::ConfirmSensitiveEntryAccess(
    const url::Origin& origin,
    const PathInfo& path_info,
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

bool FakeFileSystemAccessPermissionContext::IsFileTypeDangerous(
    const base::FilePath& path,
    const url::Origin& origin) {
  return false;
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
    const PathInfo& path_info) {
  id_pathinfo_map_.insert({id, path_info});
}

PathInfo FakeFileSystemAccessPermissionContext::GetLastPickedDirectory(
    const url::Origin& origin,
    const std::string& id) {
  return base::Contains(id_pathinfo_map_, id) ? id_pathinfo_map_[id]
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
  return base::Contains(well_known_directory_map_, directory)
             ? well_known_directory_map_[directory]
             : base::FilePath();
}

std::u16string FakeFileSystemAccessPermissionContext::GetPickerTitle(
    const blink::mojom::FilePickerOptionsPtr& options) {
  return kPickerTitle;
}

void FakeFileSystemAccessPermissionContext::NotifyEntryMoved(
    const url::Origin& origin,
    const PathInfo& old_path,
    const PathInfo& new_path) {}

void FakeFileSystemAccessPermissionContext::OnFileCreatedFromShowSaveFilePicker(
    const GURL& file_picker_binding_context,
    const storage::FileSystemURL& url) {}

void FakeFileSystemAccessPermissionContext::CheckPathsAgainstEnterprisePolicy(
    std::vector<PathInfo> entries,
    GlobalRenderFrameHostId frame_id,
    EntriesAllowedByEnterprisePolicyCallback callback) {
  std::move(callback).Run(std::move(entries));
}

}  // namespace content
