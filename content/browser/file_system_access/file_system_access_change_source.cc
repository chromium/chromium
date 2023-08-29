// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_change_source.h"

#include "base/functional/callback.h"

namespace content {

FileSystemAccessChangeSource::FileSystemAccessChangeSource(
    FileSystemAccessWatchScope scope)
    : scope_(std::move(scope)) {}

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
    base::OnceCallback<void(bool)> on_source_initialized) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (initialization_result_.has_value()) {
    CHECK(initialization_callbacks_.empty());
    std::move(on_source_initialized).Run(*initialization_result_);
    return;
  }

  initialization_callbacks_.push_back(std::move(on_source_initialized));
  if (initialization_callbacks_.size() > 1) {
    return;
  }

  Initialize(base::BindOnce(&FileSystemAccessChangeSource::DidInitialize,
                            weak_factory_.GetWeakPtr()));
}

void FileSystemAccessChangeSource::DidInitialize(bool result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!initialization_result_.has_value());
  CHECK(!initialization_callbacks_.empty());

  initialization_result_ = result;

  // Move the callbacks to the stack since they may cause |this| to be deleted.
  auto initialization_callbacks = std::move(initialization_callbacks_);
  initialization_callbacks_.clear();
  for (auto& callback : initialization_callbacks) {
    std::move(callback).Run(result);
  }
}

base::WeakPtr<FileSystemAccessChangeSource>
FileSystemAccessChangeSource::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void FileSystemAccessChangeSource::NotifyOfChange(
    const base::FilePath& relative_path,
    bool error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observers_) {
    observer.OnRawChange(this, relative_path, error);
  }
}

}  // namespace content
