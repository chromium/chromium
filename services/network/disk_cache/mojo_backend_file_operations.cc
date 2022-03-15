// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/disk_cache/mojo_backend_file_operations.h"

#include "base/task/sequenced_task_runner.h"

namespace network {

MojoBackendFileOperations::MojoBackendFileOperations(
    mojo::PendingRemote<mojom::HttpCacheBackendFileOperations> pending_remote,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : remote_(std::move(pending_remote), std::move(task_runner)) {}
MojoBackendFileOperations::~MojoBackendFileOperations() = default;

bool MojoBackendFileOperations::CreateDirectory(const base::FilePath& path) {
  bool result = false;
  remote_->CreateDirectory(path, &result);
  return result;
}

bool MojoBackendFileOperations::PathExists(const base::FilePath& path) {
  bool result = false;
  remote_->PathExists(path, &result);
  return result;
}

base::File MojoBackendFileOperations::OpenFile(const base::FilePath& path,
                                               uint32_t flags) {
  base::File file;
  base::File::Error error;
  // The value will be checked in the message receiver side.
  auto flags_to_pass = static_cast<mojom::HttpCacheBackendOpenFileFlags>(flags);
  remote_->OpenFile(path, flags_to_pass, &file, &error);
  if (error != base::File::FILE_OK) {
    return base::File(error);
  }
  return file;
}

bool MojoBackendFileOperations::DeleteFile(const base::FilePath& path) {
  bool result = false;
  remote_->DeleteFile(path, &result);
  return result;
}

bool MojoBackendFileOperations::ReplaceFile(const base::FilePath& from_path,
                                            const base::FilePath& to_path,
                                            base::File::Error* error_out) {
  base::File::Error error;
  remote_->RenameFile(from_path, to_path, &error);
  if (error_out) {
    *error_out = error;
  }
  return error == base::File::FILE_OK;
}

absl::optional<base::File::Info> MojoBackendFileOperations::GetFileInfo(
    const base::FilePath& path) {
  absl::optional<base::File::Info> info;
  remote_->GetFileInfo(path, &info);
  return info;
}

}  // namespace network
