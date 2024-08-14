// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_change_source.h"

#include "base/functional/callback.h"

namespace content {

namespace {

storage::FileSystemURL ToFileSystemURL(storage::FileSystemContext& context,
                                       const storage::FileSystemURL& root_url,
                                       const base::FilePath& relative_path) {
  auto result = context.CreateCrackedFileSystemURL(
      root_url.storage_key(), root_url.mount_type(),
      root_url.virtual_path().Append(relative_path));
  if (root_url.bucket()) {
    result.SetBucket(root_url.bucket().value());
  }
  return result;
}

}  // namespace

FileSystemAccessChangeSource::FileSystemAccessChangeSource(
    FileSystemAccessWatchScope scope,
    scoped_refptr<storage::FileSystemContext> file_system_context)
    : scope_(std::move(scope)),
      file_system_context_(std::move(file_system_context)) {}

FileSystemAccessChangeSource::~FileSystemAccessChangeSource() {
  for (auto& observer : observers_) {
    observer.OnSourceBeingDestroyed(this);
  }
}

void FileSystemAccessChangeSource::AddObserver(RawChangeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}
void FileSystemAccessChangeSource::RemoveObserver(RawChangeObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void FileSystemAccessChangeSource::EnsureInitialized(
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
        on_source_initialized) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (initialization_result_.has_value()) {
    CHECK(initialization_callbacks_.empty());
    std::move(on_source_initialized).Run(initialization_result_->Clone());
    return;
  }

  initialization_callbacks_.push_back(std::move(on_source_initialized));
  if (initialization_callbacks_.size() > 1) {
    return;
  }

  Initialize(base::BindOnce(&FileSystemAccessChangeSource::DidInitialize,
                            weak_factory_.GetWeakPtr()));
}

void FileSystemAccessChangeSource::DidInitialize(
    blink::mojom::FileSystemAccessErrorPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!initialization_result_.has_value());
  CHECK(!initialization_callbacks_.empty());

  initialization_result_ = std::move(result);

  // Move the callbacks to the stack since they may cause |this| to be deleted.
  auto initialization_callbacks = std::move(initialization_callbacks_);
  initialization_callbacks_.clear();
  for (auto& callback : initialization_callbacks) {
    std::move(callback).Run(initialization_result_->Clone());
  }
}

base::WeakPtr<FileSystemAccessChangeSource>
FileSystemAccessChangeSource::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void FileSystemAccessChangeSource::NotifyOfChange(
    const storage::FileSystemURL& changed_url,
    bool error,
    const ChangeInfo& change_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(scope().Contains(changed_url));
  CHECK(changed_url.is_valid());

  for (RawChangeObserver& observer : observers_) {
    observer.OnRawChange(changed_url, error, change_info, scope());
  }
}

void FileSystemAccessChangeSource::NotifyOfChange(
    const base::FilePath& relative_path,
    bool error,
    const ChangeInfo& change_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!relative_path.IsAbsolute());
  CHECK(!relative_path.ReferencesParent());

  const storage::FileSystemURL& root_url = scope().root_url();
  CHECK(root_url.is_valid());

  for (RawChangeObserver& observer : observers_) {
    observer.OnRawChange(
        ToFileSystemURL(*file_system_context_, root_url, relative_path), error,
        change_info, scope());
  }
}

}  // namespace content
