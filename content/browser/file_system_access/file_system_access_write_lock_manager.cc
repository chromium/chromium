// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_write_lock_manager.h"

namespace content {

FileSystemAccessWriteLockManager::WriteLock::WriteLock(
    base::WeakPtr<FileSystemAccessWriteLockManager> lock_manager,
    const storage::FileSystemURL& url,
    const WriteLockType& type,
    base::PassKey<FileSystemAccessWriteLockManager> /*pass_key*/)
    : lock_manager_(lock_manager), url_(url), type_(type) {}

FileSystemAccessWriteLockManager::WriteLock::~WriteLock() {
  if (lock_manager_)
    lock_manager_->ReleaseLock(url_);
}

FileSystemAccessWriteLockManager::FileSystemAccessWriteLockManager(
    base::PassKey<FileSystemAccessManagerImpl> /*pass_key*/) {}

FileSystemAccessWriteLockManager::~FileSystemAccessWriteLockManager() = default;

absl::optional<scoped_refptr<FileSystemAccessWriteLockManager::WriteLock>>
FileSystemAccessWriteLockManager::TakeLock(const storage::FileSystemURL& url,
                                           WriteLockType lock_type) {
  auto lock_it = locks_.find(url);
  WriteLock* existing_lock =
      lock_it != locks_.end() ? lock_it->second : nullptr;

  if (!existing_lock) {
    // There are no locks on the file, we can take any type of lock.
    WriteLock* new_lock =
        new WriteLock(weak_factory_.GetWeakPtr(), url, lock_type, PassKey());
    // The lock is owned by the caller, a raw pointer is stored `locks_` to be
    // able to increase the refcount when a new shared lock is requested on a
    // URL that has an existing one.
    //
    // It is safe to store raw pointers in `locks_` because when a lock is
    // destroyed, it's entry in the map is erased. This means that any raw
    // pointer in the map points to a valid object, and is therefore safe to
    // dereference.
    locks_.emplace(url, new_lock);

    return base::WrapRefCounted<WriteLock>(new_lock);
  }

  if (lock_type == WriteLockType::kExclusive ||
      existing_lock->type() == WriteLockType::kExclusive) {
    // There is an existing lock, and either it or the requested lock is
    // exclusive. Therefore it is not possible to take a new lock.
    return absl::nullopt;
  }

  // There is an existing shared lock, and the requested lock is also shared.
  // We increase the refcount of the existing lock by taking and returning a
  // scoped_refptr to it.
  return base::WrapRefCounted<WriteLock>(existing_lock);
}

void FileSystemAccessWriteLockManager::ReleaseLock(
    const storage::FileSystemURL& url) {
  size_t count_removed = locks_.erase(url);

  DCHECK_EQ(1u, count_removed);
}

}  // namespace content
