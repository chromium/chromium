// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_lock_manager.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/types/optional_ref.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

using LockHandle = FileSystemAccessLockManager::LockHandle;
using LockType = FileSystemAccessLockManager::LockType;

// This class represents an active lock on the `path_component`. The lock is
// kept alive when there is some `LockHandle` to it. The lock is released when
// all its `LockHandle`s have been destroyed.
class Lock {
 public:
  Lock(const base::FilePath::StringType& path_component,
       const LockType& type,
       const LockType& exclusive_lock_type,
       base::optional_ref<Lock> parent_lock)
      : path_component_(path_component),
        type_(type),
        exclusive_lock_type_(exclusive_lock_type),
        parent_lock_(std::move(parent_lock)) {}

  virtual ~Lock() = default;

  Lock(Lock const&) = delete;
  Lock& operator=(Lock const&) = delete;

  const base::FilePath::StringType& path_component() const {
    return path_component_;
  }

  const LockType& type() const { return type_; }

  bool IsExclusive() const { return type_ == exclusive_lock_type_; }

  // Returns whether this lock is contentious with `type`.
  bool IsContentious(LockType type) { return type != type_ || IsExclusive(); }

  Lock* GetChild(const base::FilePath::StringType& path_component) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    auto child_lock_it = child_locks_.find(path_component);
    return child_lock_it != child_locks_.end() ? child_lock_it->second.get()
                                               : nullptr;
  }

  // Get the child if it exists. If it doesn't, creates it if it can. Otherwise
  // return null.
  Lock* GetOrCreateChild(const base::FilePath::StringType& path_component,
                         LockType lock_type) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Lock* child = GetChild(path_component);

    if (!child) {
      return CreateChild(path_component, lock_type);
    }

    if (!child->IsContentious(lock_type)) {
      return child;
    }

    return nullptr;
  }

  scoped_refptr<LockHandle> CreateLockHandle() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!lock_handle_) {
      // The lock handle is owned by the caller or the lock handle of a child
      // lock. A raw pointer is stored in `lock_handle_` to be able to increase
      // the refcount when a new lock handle is created for this.
      //
      // It is safe to store a raw pointer in `lock_handle_` because when it is
      // destroyed, `this` is destroyed. This means that it will be a valid
      // object for the lifetime of `Lock`, and is therefore safe to
      // dereference.
      scoped_refptr<LockHandle> parent_lock_handle =
          parent_lock_.has_value() ? parent_lock_->CreateLockHandle() : nullptr;
      lock_handle_ =
          new LockHandle(weak_factory_.GetWeakPtr(), parent_lock_handle);
    }

    return base::WrapRefCounted<LockHandle>(lock_handle_);
  }

 protected:
  virtual void DestroySelf() { parent_lock_->ReleaseChild(path_component_); }

 private:
  friend class FileSystemAccessLockManager::LockHandle;

  Lock* CreateChild(const base::FilePath::StringType& path_component,
                    LockType lock_type) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Lock* child_lock =
        new Lock(path_component, lock_type, exclusive_lock_type_, this);
    child_locks_.emplace(path_component, base::WrapUnique<Lock>(child_lock));
    return child_lock;
  }

  void ReleaseChild(const base::FilePath::StringType& path_component) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    size_t count_removed = child_locks_.erase(path_component);

    CHECK_EQ(1u, count_removed);
  }

  // Called by a `LockHandle` when its destroyed.
  void LockHandleDestroyed() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // `DestroySelf` will destroy `this`.
    DestroySelf();
  }

  SEQUENCE_CHECKER(sequence_checker_);

  // The handle that holds this lock.
  raw_ptr<LockHandle> lock_handle_ = nullptr;

  // The file path component of what we're locking within our parent `Lock`.
  const base::FilePath::StringType path_component_;

  const LockType type_;

  const LockType exclusive_lock_type_;

  // The parent `Lock` which created `this`. May not hold a value if this
  // instance represents the root of its file system. When it is not null, it is
  // safe to dereference since `parent_lock_` owns `this`.
  base::optional_ref<Lock> parent_lock_;

  // The map of path components to the respective children.
  std::map<base::FilePath::StringType, std::unique_ptr<Lock>> child_locks_;

  base::WeakPtrFactory<Lock> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

class RootLock : public Lock {
 public:
  explicit RootLock(
      base::WeakPtr<FileSystemAccessLockManager> lock_manager,
      const FileSystemAccessLockManager::RootLocator& root_locator)
      : Lock({},
             lock_manager->ancestor_lock_type_,
             lock_manager->exclusive_lock_type_,
             /*parent_lock=*/absl::nullopt),
        lock_manager_(lock_manager),
        root_locator_(root_locator) {}

 private:
  void DestroySelf() override { lock_manager_->ReleaseRoot(root_locator_); }

  base::WeakPtr<FileSystemAccessLockManager> lock_manager_;
  FileSystemAccessLockManager::RootLocator root_locator_;
};

// static
FileSystemAccessLockManager::RootLocator
FileSystemAccessLockManager::RootLocator::FromFileSystemURL(
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
  return RootLocator(path_type, maybe_bucket_locator);
}

FileSystemAccessLockManager::RootLocator::RootLocator(
    const EntryPathType& type,
    const absl::optional<storage::BucketLocator>& bucket_locator)
    : type(type), bucket_locator(bucket_locator) {
  // Files in the sandboxed file system must have a `bucket_locator`. See the
  // comment in `RootLocator::FromFileSystemURL()`. Files outside of the
  // sandboxed file system should not be keyed by StorageKey to ensure that
  // locks apply across sites. i.e. separate sites cannot hold their own
  // exclusive locks to the same file.
  DCHECK_EQ(type == EntryPathType::kSandboxed, bucket_locator.has_value());
}
FileSystemAccessLockManager::RootLocator::RootLocator(const RootLocator&) =
    default;
FileSystemAccessLockManager::RootLocator::~RootLocator() = default;

bool FileSystemAccessLockManager::RootLocator::operator<(
    const RootLocator& other) const {
  return std::tie(type, bucket_locator) <
         std::tie(other.type, other.bucket_locator);
}

LockHandle::LockHandle(base::WeakPtr<Lock> lock,
                       scoped_refptr<LockHandle> parent_lock_handle)
    : lock_(lock),
      type_(lock->type()),
      is_exclusive_(lock->IsExclusive()),
      parent_lock_handle_(std::move(parent_lock_handle)) {}

LockHandle::~LockHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(lock_);
  // May destroy `lock_`.
  lock_->LockHandleDestroyed();
}

FileSystemAccessLockManager::FileSystemAccessLockManager(
    base::PassKey<FileSystemAccessManagerImpl> /*pass_key*/) {}

FileSystemAccessLockManager::~FileSystemAccessLockManager() = default;

bool FileSystemAccessLockManager::IsContentious(
    const storage::FileSystemURL& url,
    LockType lock_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Lock* lock = GetRootLock(RootLocator::FromFileSystemURL(url));
  if (!lock) {
    // If there's no root lock, then it's not contentious.
    return false;
  }

  auto base_component = url.path().BaseName().value();
  for (const auto& component : url.path().GetComponents()) {
    LockType component_lock_type =
        component == base_component ? lock_type : ancestor_lock_type_;
    lock = lock->GetChild(component);
    if (!lock) {
      // If there's no lock, then it's not contentious.
      return false;
    } else if (lock->IsContentious(component_lock_type)) {
      return true;
    }
  }

  // Nothing along the path was contentious, so there is no contention.
  return false;
}

void FileSystemAccessLockManager::TakeLock(const storage::FileSystemURL& url,
                                           LockType lock_type,
                                           TakeLockCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // GetOrCreateRootLock should always succeed.
  Lock* lock = GetOrCreateRootLock(RootLocator::FromFileSystemURL(url));
  CHECK(lock);

  // Attempt to take a lock on all components in the path.
  auto base_component = url.path().BaseName().value();
  for (const auto& component : url.path().GetComponents()) {
    // Take `lock_type` on the base and ancestor locks on the ancestors.
    LockType component_lock_type =
        component == base_component ? lock_type : ancestor_lock_type_;
    lock = lock->GetOrCreateChild(component, component_lock_type);

    if (!lock) {
      // Couldn't take lock due to contention with a lock in `url`'s path held
      // by an active page.
      //
      // No locks have been created yet, so no cleanup is necessary.
      std::move(callback).Run(nullptr);
      return;
    }
  }

  // Successfully created the lock and passing a lock handle to the callback.
  std::move(callback).Run(lock->CreateLockHandle());
}

void FileSystemAccessLockManager::ReleaseRoot(const RootLocator& root_locator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t count_removed = root_locks_.erase(root_locator);

  DCHECK_EQ(1u, count_removed);
}

RootLock* FileSystemAccessLockManager::GetRootLock(
    const RootLocator& root_locator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto root_lock_it = root_locks_.find(root_locator);
  return root_lock_it != root_locks_.end() ? root_lock_it->second.get()
                                           : nullptr;
}

RootLock* FileSystemAccessLockManager::GetOrCreateRootLock(
    const RootLocator& root_locator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RootLock* root_lock = GetRootLock(root_locator);
  if (!root_lock) {
    root_lock = new RootLock(weak_factory_.GetWeakPtr(), root_locator);
    root_locks_.emplace(std::move(root_locator),
                        base::WrapUnique<RootLock>(root_lock));
  }
  return root_lock;
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
