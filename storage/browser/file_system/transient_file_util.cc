// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/transient_file_util.h"

#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"

namespace storage {

namespace {

void RevokeFileSystem(const std::string& filesystem_id,
                      const base::FilePath& /*path*/) {
  IsolatedContext::GetInstance()->RevokeFileSystem(filesystem_id);
}

}  // namespace

ScopedFile TransientFileUtil::CreateSnapshotFile(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    base::File::Error* error,
    base::File::Info* file_info,
    base::FilePath* platform_path) {
  DCHECK(file_info);
  *error = GetFileInfo(context, url, file_info, platform_path);
  if (*error == base::File::FILE_OK && file_info->is_directory)
    *error = base::File::FILE_ERROR_NOT_A_FILE;
  if (*error != base::File::FILE_OK)
    return ScopedFile();

  // Sets-up a transient filesystem.
  DCHECK(!platform_path->empty());
  DCHECK(!url.filesystem_id().empty());

  ScopedFile scoped_file(*platform_path, ScopedFile::DELETE_ON_SCOPE_OUT,
                         context->task_runner());
  scoped_file.AddScopeOutCallback(
      base::BindOnce(&RevokeFileSystem, url.filesystem_id()), nullptr);

  return scoped_file;
}

}  // namespace storage
