// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_write_lock_manager.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

using EntryLocator = FileSystemAccessWriteLockManager::EntryLocator;
using WriteLock = FileSystemAccessWriteLockManager::WriteLock;

// static
EntryLocator EntryLocator::FromFileSystemURL(
    const storage::FileSystemURL& url) {
  EntryPathType path_type;
  switch (url.type()) {
    case storage::kFileSystemTypeLocal:
    case storage::kFileSystemTypeTest:
      path_type = EntryPathType::kLocal;
      break;
    case storage::kFileSystemTypeTemporary:
      path_type = EntryPathType::kSandboxed;
      break;
    default:
      DCHECK_EQ(url.mount_type(),
                storage::FileSystemType::kFileSystemTypeExternal);
      path_type = EntryPathType::kExternal;
  }
  base::FilePath path =
      path_type == EntryPathType::kLocal ? url.path() : url.virtual_path();
  // Sandboxed file system URLs may or may not have a bucket locator. If they
  // don't, construct a dummy bucket locator and populate it with the URL's
  // storage key to ensure that files of the same relative path in different
  // sandboxed file systems represent distinct locks. Note that files in the
  // same file system can use different locks if one has bucket information and
  // one does not, though we don't expect this to happen in practice...
  //
  // TODO(crbug.com/1329927): If/when we require all FileSystemURLs in sandboxed
  // file systems to have a bucket locator, replace this logic with a DCHECK.
  absl::optional<storage::BucketLocator> maybe_bucket_locator = absl::nullopt;
  if (path_type == EntryPathType::kSandboxed) {
    storage::BucketLocator bucket_locator;
    if (url.bucket().has_value()) {
      bucket_locator = url.bucket().value();
    } else {
      bucket_locator.storage_key = url.storage_key();
    }
    maybe_bucket_locator.emplace(std::move(bucket_locator));
  }
  return EntryLocator(path_type, path, maybe_bucket_locator);
}

EntryLocator::EntryLocator(
    const EntryPathType& type,
    const base::FilePath& path,
    const absl::optional<storage::BucketLocator>& bucket_locator)
    : type(type), path(path), bucket_locator(bucket_locator) {
  // Files in the sandboxed file system must have a `bucket_locator`. See the
  // comment in `EntryLocator::FromFileSystemURL()`. Files outside of the
  // sandboxed file system should not be keyed by StorageKey to ensure that
  // separate sites cannot hold their own exclusive locks to the same file.
  DCHECK_EQ(type == EntryPathType::kSandboxed, bucket_locator.has_value());
}
EntryLocator::EntryLocator(const EntryLocator&) = default;
EntryLocator::~EntryLocator() = default;

bool EntryLocator::operator<(const EntryLocator& other) const {
  return std::tie(type, path, bucket_locator) <
         std::tie(other.type, other.path, other.bucket_locator);
}

WriteLock::WriteLock(
    base::WeakPtr<FileSystemAccessWriteLockManager> lock_manager,
    const EntryLocator& entry_locator,
    const WriteLockType& type,
    const scoped_refptr<WriteLock> parent_lock,
    base::PassKey<FileSystemAccessWriteLockManager> pass_key)
    : lock_manager_(lock_manager),
      entry_locator_(entry_locator),
      type_(type),
      parent_lock_(std::move(parent_lock)) {}

WriteLock::~WriteLock() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lock_manager_)
    lock_manager_->ReleaseLock(entry_locator_);
}

FileSystemAccessWriteLockManager::FileSystemAccessWriteLockManager(
    base::PassKey<FileSystemAccessManagerImpl> /*pass_key*/) {}

FileSystemAccessWriteLockManager::~FileSystemAccessWriteLockManager() = default;

scoped_refptr<WriteLock> FileSystemAccessWriteLockManager::TakeLock(
    const storage::FileSystemURL& url,
    WriteLockType lock_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  EntryLocator entry_locator = EntryLocator::FromFileSystemURL(url);
  return TakeLockImpl(entry_locator, lock_type);
}

scoped_refptr<WriteLock> FileSystemAccessWriteLockManager::TakeLockImpl(
    const EntryLocator& entry_locator,
    WriteLockType lock_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto lock_it = locks_.find(entry_locator);
  WriteLock* existing_lock =
      lock_it != locks_.end() ? lock_it->second : nullptr;

  if (!existing_lock) {
    // Recursively try to acquire shared locks on all parent directories. If any
    // parent directories are locked, lock acquisition should fail.
    scoped_refptr<WriteLock> parent_lock;
    auto parent_path = entry_locator.path.DirName();
    if (parent_path != entry_locator.path) {
      EntryLocator parent_entry_locator{entry_locator.type, parent_path,
                                        entry_locator.bucket_locator};
      parent_lock = TakeLockImpl(parent_entry_locator, WriteLockType::kShared);
      if (!parent_lock)
        return nullptr;
    }

    // There are no locks on the file, we can take any type of lock.
    WriteLock* new_lock =
        new WriteLock(weak_factory_.GetWeakPtr(), entry_locator, lock_type,
                      std::move(parent_lock), PassKey());
    // The lock is owned by the caller or its child lock. A raw pointer is
    // stored `locks_` to be able to increase the refcount when a new shared
    // lock is requested on a URL that has an existing one.
    //
    // It is safe to store raw pointers in `locks_` because when a lock is
    // destroyed, it's entry in the map is erased. This means that any raw
    // pointer in the map points to a valid object, and is therefore safe to
    // dereference.
    locks_.emplace(std::move(entry_locator), new_lock);

    return base::WrapRefCounted<WriteLock>(new_lock);
  }

  if (lock_type == WriteLockType::kExclusive ||
      existing_lock->type() == WriteLockType::kExclusive) {
    // There is an existing lock, and either it or the requested lock is
    // exclusive. Therefore it is not possible to take a new lock.
    return nullptr;
  }

  // There is an existing shared lock, and the requested lock is also shared.
  // We increase the refcount of the existing lock by taking and returning a
  // scoped_refptr to it.
  return base::WrapRefCounted<WriteLock>(existing_lock);
}

void FileSystemAccessWriteLockManager::ReleaseLock(
    const EntryLocator& entry_locator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t count_removed = locks_.erase(entry_locator);

  DCHECK_EQ(1u, count_removed);
}

}  // namespace content
