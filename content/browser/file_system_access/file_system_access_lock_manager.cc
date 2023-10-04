// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_lock_manager.h"
#include "base/files/file_path.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

using EntryLocator = FileSystemAccessLockManager::EntryLocator;
using LockHandle = FileSystemAccessLockManager::LockHandle;
using LockType = FileSystemAccessLockManager::LockType;

// This class represents an active lock on an entry locator. The lock is kept
// alive when there is some `LockHandle` to it. The lock is released when all
// its `LockHandle`s have been destroyed.
class Lock {
 public:
  Lock(base::WeakPtr<FileSystemAccessLockManager> lock_manager,
       const EntryLocator& entry_locator,
       const LockType& type,
       scoped_refptr<LockHandle> parent_lock)
      : lock_manager_(lock_manager),
        entry_locator_(entry_locator),
        type_(type),
        parent_lock_(std::move(parent_lock)) {}

  ~Lock() = default;

  Lock(Lock const&) = delete;
  Lock& operator=(Lock const&) = delete;

  const EntryLocator& locator() const { return entry_locator_; }

  const LockType& type() const { return type_; }

  bool IsExclusive() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return type_ == lock_manager_->exclusive_lock_type_;
  }

  // Returns whether this lock is contentious with `type`.
  bool IsContentious(LockType type) { return type != type_ || IsExclusive(); }

  scoped_refptr<LockHandle> CreateLockHandle() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!lock_handle_) {
      // The lock is owned by the caller or its child lock. A raw pointer is
      // stored in `lock_handle_` to be able to increase the refcount when a new
      // shared lock is requested for this `Lock`.
      //
      // It is safe to store raw pointers in `lock_handle_` because when a lock
      // is destroyed, `this` is destroyed. This means that it will be a valid
      // object for the lifetime of `Lock`, and is therefore safe to
      // dereference.
      lock_handle_ = new LockHandle(weak_factory_.GetWeakPtr());
    }

    return base::WrapRefCounted<LockHandle>(lock_handle_);
  }

 private:
  friend class FileSystemAccessLockManager::LockHandle;

  SEQUENCE_CHECKER(sequence_checker_);

  // Called by a `LockHandle` when its destroyed.
  void LockHandleDestroyed() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // ReleaseLock will destroy `this`.
    lock_manager_->ReleaseLock(entry_locator_);
  }

  // The FileSystemAccessLockManager that created this instance. Used on
  // destruction to release the lock on the file.
  base::WeakPtr<FileSystemAccessLockManager> lock_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The handle that holds this lock.
  raw_ptr<LockHandle> lock_handle_ = nullptr;

  // Locator of the file or directory associated with this lock. It is used to
  // unlock the lock on release.
  const EntryLocator entry_locator_;

  const LockType type_;

  // When a file or directory is locked, it acquires a shared lock on its
  // parent directory, which acquires a shared lock on its parent, and so
  // forth. When this instance goes away, the associated ancestor locks are
  // automatically released. May be null if this instance represents the root
  // of its file system.
  const scoped_refptr<LockHandle> parent_lock_;

  base::WeakPtrFactory<Lock> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

// static
EntryLocator EntryLocator::FromFileSystemURL(
    const storage::FileSystemURL& url) {
  absl::optional<storage::BucketLocator> maybe_bucket_locator = absl::nullopt;
  EntryPathType path_type;
  switch (url.type()) {
    case storage::kFileSystemTypeLocal:
    case storage::kFileSystemTypeTest:
      DCHECK(!url.bucket());
      path_type = EntryPathType::kLocal;
      break;
    case storage::kFileSystemTypeTemporary:
      // URLs from the sandboxed file system must include bucket information.
      DCHECK(url.bucket());
      maybe_bucket_locator = url.bucket().value();
      path_type = EntryPathType::kSandboxed;
      break;
    default:
      DCHECK(!url.bucket());
      DCHECK_EQ(url.mount_type(),
                storage::FileSystemType::kFileSystemTypeExternal);
      path_type = EntryPathType::kExternal;
  }
  return EntryLocator(path_type, url.path(), maybe_bucket_locator);
}

EntryLocator::EntryLocator(
    const EntryPathType& type,
    const base::FilePath& path,
    const absl::optional<storage::BucketLocator>& bucket_locator)
    : type(type), path(path), bucket_locator(bucket_locator) {
  // Files in the sandboxed file system must have a `bucket_locator`. See the
  // comment in `EntryLocator::FromFileSystemURL()`. Files outside of the
  // sandboxed file system should not be keyed by StorageKey to ensure that
  // locks apply across sites. i.e. separate sites cannot hold their own
  // exclusive locks to the same file.
  DCHECK_EQ(type == EntryPathType::kSandboxed, bucket_locator.has_value());
}
EntryLocator::EntryLocator(const EntryLocator&) = default;
EntryLocator::~EntryLocator() = default;

bool EntryLocator::operator<(const EntryLocator& other) const {
  return std::tie(type, path, bucket_locator) <
         std::tie(other.type, other.path, other.bucket_locator);
}

LockHandle::LockHandle(base::WeakPtr<Lock> lock)
    : lock_(lock), type_(lock->type()), is_exclusive_(lock->IsExclusive()) {}

LockHandle::~LockHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (lock_) {
    lock_->LockHandleDestroyed();
  }
}

FileSystemAccessLockManager::FileSystemAccessLockManager(
    base::PassKey<FileSystemAccessManagerImpl> /*pass_key*/) {}

FileSystemAccessLockManager::~FileSystemAccessLockManager() = default;

scoped_refptr<LockHandle> FileSystemAccessLockManager::TakeLock(
    const storage::FileSystemURL& url,
    LockType lock_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  EntryLocator entry_locator = EntryLocator::FromFileSystemURL(url);
  return TakeLockImpl(entry_locator, lock_type);
}

scoped_refptr<LockHandle> FileSystemAccessLockManager::TakeLockImpl(
    const EntryLocator& entry_locator,
    LockType lock_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto lock_it = locks_.find(entry_locator);
  Lock* existing_lock =
      lock_it != locks_.end() ? lock_it->second.get() : nullptr;

  if (!existing_lock) {
    // Recursively try to acquire shared locks on all parent directories. If any
    // parent directories are locked, lock acquisition should fail.
    scoped_refptr<LockHandle> parent_lock;
    auto parent_path = entry_locator.path.DirName();
    if (parent_path != entry_locator.path) {
      EntryLocator parent_entry_locator{entry_locator.type, parent_path,
                                        entry_locator.bucket_locator};
      parent_lock = TakeLockImpl(parent_entry_locator, ancestor_lock_type_);
      if (!parent_lock) {
        return nullptr;
      }
    }

    // There are no locks on the file, we can take any type of lock.
    std::unique_ptr<Lock> new_lock =
        std::make_unique<Lock>(weak_factory_.GetWeakPtr(), entry_locator,
                               lock_type, std::move(parent_lock));

    // The lock handle is owned by the caller or its child lock.
    scoped_refptr<LockHandle> lock_handle = new_lock->CreateLockHandle();

    // The lock is stored in `locks_` for future calls of `TakeLockImpl` to get
    // the existing lock.
    locks_.emplace(std::move(entry_locator), std::move(new_lock));

    return lock_handle;
  }

  if (lock_type != existing_lock->type() || lock_type == exclusive_lock_type_) {
    // There is an existing lock, and either it's not the same type as the
    // requested lock or it's an exclusive lock. Therefore it is not possible to
    // take a new lock.
    return nullptr;
  }

  // The existing lock is not in contention with the requested lock, so return a
  // handle to it.
  return existing_lock->CreateLockHandle();
}

void FileSystemAccessLockManager::ReleaseLock(
    const EntryLocator& entry_locator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t count_removed = locks_.erase(entry_locator);

  DCHECK_EQ(1u, count_removed);
}

FileSystemAccessLockManager::LockType
FileSystemAccessLockManager::CreateSharedLockType() {
  return lock_type_generator_.GenerateNextId();
}

FileSystemAccessLockManager::LockType
FileSystemAccessLockManager::GetExclusiveLockType() {
  return exclusive_lock_type_;
}

FileSystemAccessLockManager::LockType
FileSystemAccessLockManager::GetAncestorLockTypeForTesting() {
  return ancestor_lock_type_;
}

}  // namespace content
