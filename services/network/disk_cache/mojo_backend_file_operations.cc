// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/disk_cache/mojo_backend_file_operations.h"

#include "base/task/sequenced_task_runner.h"

namespace network {

namespace {

class FileEnumeratorImpl final
    : public disk_cache::BackendFileOperations::FileEnumerator {
 public:
  using FileEnumerationEntry =
      disk_cache::BackendFileOperations::FileEnumerationEntry;

  explicit FileEnumeratorImpl(mojo::PendingRemote<mojom::FileEnumerator> remote)
      : remote_(std::move(remote)) {}
  ~FileEnumeratorImpl() override = default;

  std::optional<FileEnumerationEntry> Next() override {
    if (has_seen_end_) {
      return std::nullopt;
    }
    if (index_ >= entries_.size()) {
      index_ = 0;
      entries_.clear();
      remote_->GetNext(kNumEntries, &entries_, &has_seen_end_, &has_error_);
    }
    if (entries_.empty()) {
      if (has_seen_end_) {
        return std::nullopt;
      }
      has_error_ = true;
      has_seen_end_ = true;
      return std::nullopt;
    }
    DCHECK_LT(index_, entries_.size());
    return std::make_optional<FileEnumerationEntry>(
        std::move(entries_[index_++]));
  }

  bool HasError() const override { return has_error_; }

 private:
  mojo::Remote<mojom::FileEnumerator> remote_;
  std::vector<FileEnumerationEntry> entries_;
  bool has_seen_end_ = false;
  bool has_error_ = false;
  size_t index_ = 0;

  static constexpr uint32_t kNumEntries = 1000;
};

}  // namespace

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

bool MojoBackendFileOperations::DirectoryExists(const base::FilePath& path) {
  bool result = false;
  remote_->DirectoryExists(path, &result);
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

bool MojoBackendFileOperations::DeleteFile(const base::FilePath& path,
                                           DeleteFileMode mode) {
  mojom::HttpCacheBackendDeleteFileMode mode_to_pass;
  switch (mode) {
    case DeleteFileMode::kDefault:
      mode_to_pass = mojom::HttpCacheBackendDeleteFileMode::kDefault;
      break;
    case DeleteFileMode::kEnsureImmediateAvailability:
      mode_to_pass =
          mojom::HttpCacheBackendDeleteFileMode::kEnsureImmediateAvailability;
      break;
  }
  bool result = false;
  remote_->DeleteFile(path, mode_to_pass, &result);
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

std::optional<base::File::Info> MojoBackendFileOperations::GetFileInfo(
    const base::FilePath& path) {
  std::optional<base::File::Info> info;
  remote_->GetFileInfo(path, &info);
  return info;
}

std::unique_ptr<disk_cache::BackendFileOperations::FileEnumerator>
MojoBackendFileOperations::EnumerateFiles(const base::FilePath& path) {
  mojo::PendingRemote<mojom::FileEnumerator> remote;
  remote_->EnumerateFiles(path, remote.InitWithNewPipeAndPassReceiver());
  return std::make_unique<FileEnumeratorImpl>(std::move(remote));
}

void MojoBackendFileOperations::CleanupDirectory(
    const base::FilePath& path,
    base::OnceCallback<void(bool)> callback) {
  remote_->CleanupDirectory(path, std::move(callback));
}

std::unique_ptr<disk_cache::UnboundBackendFileOperations>
MojoBackendFileOperations::Unbind() {
  return std::make_unique<UnboundMojoBackendFileOperations>(remote_.Unbind());
}

UnboundMojoBackendFileOperations::UnboundMojoBackendFileOperations(
    mojo::PendingRemote<mojom::HttpCacheBackendFileOperations> pending_remote)
    : pending_remote_(std::move(pending_remote)) {}

UnboundMojoBackendFileOperations::~UnboundMojoBackendFileOperations() = default;

std::unique_ptr<disk_cache::BackendFileOperations>
UnboundMojoBackendFileOperations::Bind(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return std::make_unique<MojoBackendFileOperations>(std::move(pending_remote_),
                                                     std::move(task_runner));
}

}  // namespace network
