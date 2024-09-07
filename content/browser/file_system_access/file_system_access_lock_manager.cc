// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_lock_manager.h"

#include <optional>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ref.h"
#include "base/types/optional_ref.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/file_system_access/features.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/common/features_generated.h"

namespace content {

using LockHandle = FileSystemAccessLockManager::LockHandle;
using LockType = FileSystemAccessLockManager::LockType;

// This class represents an active lock on the `path`. The lock is kept alive
// when there is some `LockHandle` to it. The lock is released when all its
// `LockHandle`s have been destroyed.
//
// Terminology:
//  - A "caller" is the caller of `FileSystemAccessLockManager` `TakeLock`
//    function that requested the `Lock`. A `Lock` can have multiple callers.
//  - A "subtree" is subtree of the whole `Lock` tree. The `Lock` tree can
//    contain zero or more subtrees. Subtrees have a root called a subroot. A
//    `Lock` is considered to be in some subtree if the it is the subroot of the
//    subtree or a descendant of the subroot. A subtree can be one of two types:
//    Evicting or Pending.
//
// A `Lock` can be in one of two states: Taken or Pending.
//  - A `Lock` is Taken if any of its `LockHandle`s have been given out to the
//    callers. A `Lock` becomes Taken when it has either been created without
//    contention or it was a Pending `Lock` that was promoted to Taken.
//  - A `Lock` is Pending if none of its `LockHandle`s have not been given out
//    to its callers. Before it is promoted to a Taken `Lock`, a Pending `Lock`
//    can only be destroyed if it is evicted. A Pending `Lock` is in a Pending
//    subtree. A `Lock` is a Pending subroot when it has been created through
//    the eviction of a Taken `Lock` that has yet to be evicted. A Pending
//    `Lock` is promoted to a Taken when that evicting Taken `Lock` has finished
//    eviction.
//
// A `Lock` may be evicted to allow for the creation of a new `Lock`. This can
// only happen when the original `Lock` is held only by pages in the BFCache and
// it is in contention with the new `Lock`. The new `Lock` is created as a
// Pending `Lock` and takes the place of the original `Lock`. What happens to
// the original `Lock` depends on its state.
//  - When a Taken `Lock` is evicted, it becomes an Evicting subroot, so it and
//    its descendants are an Evicting subtree.
//  - When a Pending `Lock` is evicted, it is immediately destroyed by
//    destroying all LockHandles to it. An evicted Pending `Lock` will never
//    have its `LockHandle`s given to the caller.
class Lock {
 public:
  Lock(const base::FilePath& path,
       const LockType& type,
       const LockType& exclusive_lock_type,
       base::optional_ref<Lock> parent_lock)
      : path_(path),
        type_(type),
        exclusive_lock_type_(exclusive_lock_type),
        parent_lock_(std::move(parent_lock)),
        is_pending_(parent_lock_.has_value() &&
                    parent_lock_->InPendingSubtree()) {}

  virtual ~Lock() {
    CHECK(pending_callbacks_.empty());
    CHECK(frame_id_lock_handles_.empty());
  }

  Lock(Lock const&) = delete;
  Lock& operator=(Lock const&) = delete;

  const base::FilePath& path() const { return path_; }

  const LockType& type() const { return type_; }

  bool IsExclusive() const { return type_ == exclusive_lock_type_; }

  // Returns whether this lock is contentious with `type`.
  bool IsContentious(LockType type) { return type != type_ || IsExclusive(); }

  Lock* GetChild(const base::FilePath& path) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    auto child_lock_it = child_locks_.find(path);
    return child_lock_it != child_locks_.end() ? child_lock_it->second.get()
                                               : nullptr;
  }

  // Get the child if it exists. If it doesn't, creates it if it can. Otherwise
  // return null.
  Lock* GetOrCreateChild(const base::FilePath& path, LockType lock_type) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Lock* child = GetChild(path);

    if (!child) {
      return CreateChild(path, lock_type);
    }

    if (!child->IsContentious(lock_type)) {
      return child;
    }

    // Evict on contention is only enabled when both FSA Locking Scheme and
    // BFCache are enabled.
    bool evict_on_contention =
        base::FeatureList::IsEnabled(features::kFileSystemAccessBFCache);

    // Start eviction if we can. Otherwise, we can not take this lock since it
    // is in contention with a lock held by an active page.
    if (!evict_on_contention || !child->IsEvictableAndStartEviction()) {
      return nullptr;
    }

    // Create a child that is pending on the eviction of the current child.
    std::unique_ptr<Lock> evicting_subroot_lock = TakeChild(path);
    CHECK(evicting_subroot_lock);
    child = CreateChild(path, lock_type);
    child->SetEvictingSubrootLock(std::move(evicting_subroot_lock));

    return child;
  }

  scoped_refptr<LockHandle> CreateLockHandle(
      const GlobalRenderFrameHostId& frame_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Insert `frame_id` if needed.
    if (!frame_id_lock_handles_.contains(frame_id)) {
      // The lock handle is owned by the caller or the lock handle of a child
      // lock. A raw ref is stored in `frame_id_lock_handles_` to be able to
      // increase the refcount when a new lock handle is created for this.
      //
      // It is safe to store raw refs in `frame_id_lock_handles_` because when
      // the lock handle is destroyed, it's entry in the map is erased. This
      // means that any raw ref in the map points to a valid object, and is
      // therefore safe to dereference.
      scoped_refptr<LockHandle> parent_lock_handle =
          parent_lock_.has_value() ? parent_lock_->CreateLockHandle(frame_id)
                                   : nullptr;
      frame_id_lock_handles_.emplace(
          frame_id,
          base::raw_ref<LockHandle>::from_ptr(new LockHandle(
              weak_factory_.GetWeakPtr(), parent_lock_handle, frame_id)));
    }

    return base::WrapRefCounted<LockHandle>(
        &frame_id_lock_handles_.at(frame_id).get());
  }

  // Stores the `TakeLockCallback` bound with a `LockHandle` to `this` for
  // `frame_id`. Unless `this` is evicted, the callback will be run once our
  // `evicting_subroot_lock_` has been evicted.
  void StorePendingCallback(
      FileSystemAccessLockManager::TakeLockCallback pending_callback,
      const GlobalRenderFrameHostId& frame_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    pending_callbacks_.push_back(base::BindOnce(std::move(pending_callback),
                                                CreateLockHandle(frame_id)));
  }

  // `Lock`s in a Pending subtree are pending on becoming Taken. See class
  // comment.
  bool InPendingSubtree() { return is_pending_; }

 protected:
  virtual void DestroySelf() { parent_lock_->ReleaseChild(path_); }

 private:
  friend class FileSystemAccessLockManager::LockHandle;

  Lock* CreateChild(const base::FilePath& path, LockType lock_type) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Lock* child_lock = new Lock(path, lock_type, exclusive_lock_type_, this);
    child_locks_.emplace(path, base::WrapUnique<Lock>(child_lock));
    return child_lock;
  }

  std::unique_ptr<Lock> TakeChild(const base::FilePath& path) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto child_node = child_locks_.extract(path);
    if (child_node.empty()) {
      return nullptr;
    }
    return std::move(child_node.mapped());
  }

  void ReleaseChild(const base::FilePath& path) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    size_t count_removed = child_locks_.erase(path);

    CHECK_EQ(1u, count_removed);
  }

  bool IsHeldOnlyByInactivePages() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    for (const auto& [frame_id, _lock_handle] : frame_id_lock_handles_) {
      RenderFrameHost* rfh = RenderFrameHost::FromID(frame_id);
      // Frames without an associated render frame host (e.g. Service Workers,
      // Shared Workers) cannot be evicted from the BFCache and are therefore
      // considered active.
      if (!rfh || rfh->IsActive()) {
        return false;
      }
    }
    return true;
  }

  // Returns if this lock can be evicted. If it can, it starts evicting this
  // lock by evicting the pages that hold it.
  bool IsEvictableAndStartEviction() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (IsHeldOnlyByInactivePages()) {
      // Evict the lock holders.
      for (const auto& [frame_id, _lock_handle] : frame_id_lock_handles_) {
        RenderFrameHost* rfh = RenderFrameHost::FromID(frame_id);
        if (rfh) {
          rfh->IsInactiveAndDisallowActivation(
              content::DisallowActivationReasonId::
                  kFileSystemAccessLockingContention);
        }
      }
      return true;
    }
    return false;
  }

  // Called by a `LockHandle` when its destroyed.
  void LockHandleDestroyed(const GlobalRenderFrameHostId& frame_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(frame_id_lock_handles_.contains(frame_id));

    frame_id_lock_handles_.erase(frame_id);

    // If nothing is holding this lock, release it.
    if (frame_id_lock_handles_.empty()) {
      // If we're not an Evicting subroot, then our parent owns us, and we can
      // destroy ourselves through our parent.
      if (!IsEvictingSubroot()) {
        // `DestroySelf` will destroy `this`.
        DestroySelf();
        return;
      }

      // The root cannot be an Evicting subroot.
      CHECK(parent_lock_.has_value());

      // If we are an Evicting subroot, then we must destroy ourselves through
      // the `pending_lock` that owns us.
      Lock* pending_lock = parent_lock_->GetChild(path_);
      bool in_pending_subtree = InPendingSubtree();
      CHECK(!in_pending_subtree || !IsPendingSubroot());

      // `RemoveEvictingSubrootLock` will destroy `this`.
      pending_lock->RemoveEvictingSubrootLock();

      if (in_pending_subtree) {
        return;
      }

      if (pending_lock->InEvictingSubtree()) {
        // If the pending lock is in an Evicting subtree, then it should be
        // evicted.

        pending_lock->EvictPendingSubtree();
        return;
      }

      // This was the last Evicting Lock that needed to be destroyed to promote
      // the `pending_lock`.
      pending_lock->PromotePendingToTaken();
    }
  }

  // Iterates over all the leaves of a Pending subtree and passes their
  // `pending_callbacks_` to `callback`.
  void IteratePendingSubtreeCallbacks(
      bool stop_pending,
      const base::RepeatingCallback<void(std::vector<base::OnceClosure>)>&
          callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(InPendingSubtree());

    if (evicting_subroot_lock_) {
      evicting_subroot_lock_->EvictPendingSubtree();
    }
    if (stop_pending) {
      is_pending_ = false;
    }

    if (child_locks_.size() > 0) {
      // This is an ancestor, so it shouldn't have any Pending callbacks.
      CHECK(pending_callbacks_.size() == 0);

      // May destroy `this` since ancestors are owned by their children, and we
      // might destroy all the children.
      for (auto child_locks_iter = child_locks_.begin(),
                child_locks_end = child_locks_.end();
           child_locks_iter != child_locks_end;) {
        // The child may be destroyed so increase the iterator before
        // continuing.
        auto& [_path, child] = *(child_locks_iter++);

        // May destroy `this` if none of the leaves' `pending_callbacks` keep
        // their `LockHandle` alive.
        child->IteratePendingSubtreeCallbacks(stop_pending, callback);
      }
      return;
    }

    // This is a leaf of a Pending subtree, so it should have Pending callbacks.
    CHECK(pending_callbacks_.size() > 0);

    // `this` may be destroyed.
    callback.Run(std::move(pending_callbacks_));
  }

  void EvictPendingSubtree() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(InPendingSubtree());

    IteratePendingSubtreeCallbacks(
        /*stop_pending=*/false,
        base::BindRepeating(
            [](std::vector<base::OnceClosure> pending_callbacks) {
              pending_callbacks.clear();
            }));
  }

  // Promotes a Pending lock to a Taken lock by handing out the `LockHandle`s to
  // the leafs of the pending subtree. See class comment.
  void PromotePendingToTaken() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(InPendingSubtree());

    IteratePendingSubtreeCallbacks(
        /*stop_pending=*/true,
        base::BindRepeating(
            [](std::vector<base::OnceClosure> pending_callbacks) {
              for (auto& pending_callback : pending_callbacks) {
                // May destroy `this` if none of the `pending_callbacks`
                // keep their `LockHandle` alive.
                std::move(pending_callback).Run();
              }
            }));
  }

  // Returns if its the subroot of a Pending subtree. See class comment.
  bool IsPendingSubroot() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // At the subroot of a Pending subtree is an Evicting subroot lock. This is
    // the lock that we're Pending on being evicted before we promote this
    // Pending subtree to an existing subtree.
    return static_cast<bool>(evicting_subroot_lock_);
  }

  // `Lock`s in an Evicting subtree are held by pages that are being evicted
  // from the BFCache. See class comment.
  bool InEvictingSubtree() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return parent_lock_.has_value() &&
           (IsEvictingSubroot() || parent_lock_->InEvictingSubtree());
  }

  // Returns if we're the subroot of an Evicting subtree. See class comment.
  bool IsEvictingSubroot() {
    return parent_lock_.has_value() &&
           parent_lock_->GetChild(path_)->evicting_subroot_lock_.get() == this;
  }

  // Makes `this` a Pending subtree that is waiting on the destruction of the
  // Evicting subtree whose subroot is `evicting_subroot_lock`. See class
  // comment.
  void SetEvictingSubrootLock(std::unique_ptr<Lock> evicting_subroot_lock) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    is_pending_ = true;
    evicting_subroot_lock_ = std::move(evicting_subroot_lock);

    // If our `evicting_subroot_lock_` has its own `evicting_subroot_lock_`,
    // then replace the former with the latter.
    auto next_evicting_subroot_lock =
        evicting_subroot_lock_->TakeEvictingSubrootLock();
    if (next_evicting_subroot_lock) {
      // Destroys `evicting_subroot_lock_`.
      evicting_subroot_lock_->EvictPendingSubtree();
      evicting_subroot_lock_ = std::move(next_evicting_subroot_lock);
    }
  }

  void RemoveEvictingSubrootLock() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(evicting_subroot_lock_);

    evicting_subroot_lock_ = nullptr;
  }

  std::unique_ptr<Lock> TakeEvictingSubrootLock() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return std::move(evicting_subroot_lock_);
  }

  SEQUENCE_CHECKER(sequence_checker_);

  // A map of frame ids to the lock handle for that frame.
  base::flat_map<GlobalRenderFrameHostId, raw_ref<LockHandle>>
      frame_id_lock_handles_;

  // The file path of what we're locking within our parent `Lock`.
  const base::FilePath path_;

  const LockType type_;

  const LockType exclusive_lock_type_;

  // The parent `Lock` which created `this`. May not hold a value if this
  // instance represents the subroot of its file system. When it is not null, it
  // is safe to dereference since `parent_lock_` owns `this`.
  base::optional_ref<Lock> parent_lock_;

  // The map of path and lock to the respective children.
  std::map<base::FilePath, std::unique_ptr<Lock>> child_locks_;

  bool is_pending_;

  // When `this` is created as a Pending `Lock`, the `Lock` it evicted is
  // transferred from the parent to `this`'s `evicting_subroot_lock_`. Once
  // `evicting_subroot_lock_` is destroyed, `this` is promoted to Taken unless
  // it too has been evicted. See class comment.
  std::unique_ptr<Lock> evicting_subroot_lock_;

  // When a Pending `Lock` is created, the original callbacks passed to
  // `FileSystemAccessLockManager`'s `TakeLock` are bound with `LockHandle`s to
  // the Pending `Lock` and stored in the Pending `Lock`.
  //
  // Once the contentious Evicting `Lock` has been destroyed and if the Pending
  // `Lock` hasn't itself been evicted, they are all run. If the Pending `Lock`
  // is evicted, then the callbacks are never run.
  std::vector<base::OnceClosure> pending_callbacks_;

  base::WeakPtrFactory<Lock> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

class RootLock : public Lock {
 public:
  explicit RootLock(
      scoped_refptr<FileSystemAccessLockManager> lock_manager,
      const FileSystemAccessLockManager::RootLocator& root_locator)
      : Lock({},
             lock_manager->ancestor_lock_type_,
             lock_manager->exclusive_lock_type_,
             /*parent_lock=*/std::nullopt),
        lock_manager_(lock_manager),
        root_locator_(root_locator) {}

 private:
  void DestroySelf() override { lock_manager_->ReleaseRoot(root_locator_); }

  scoped_refptr<FileSystemAccessLockManager> lock_manager_;
  FileSystemAccessLockManager::RootLocator root_locator_;
};

// static
FileSystemAccessLockManager::RootLocator
FileSystemAccessLockManager::RootLocator::FromFileSystemURL(
    const storage::FileSystemURL& url) {
  std::optional<storage::BucketLocator> maybe_bucket_locator = std::nullopt;
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
    const std::optional<storage::BucketLocator>& bucket_locator)
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
                       scoped_refptr<LockHandle> parent_lock_handle,
                       const GlobalRenderFrameHostId& frame_id)
    : lock_(lock),
      type_(lock->type()),
      is_exclusive_(lock->IsExclusive()),
      parent_lock_handle_(std::move(parent_lock_handle)),
      frame_id_(frame_id) {}

LockHandle::~LockHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(lock_);
  // May destroy `lock_`.
  lock_->LockHandleDestroyed(frame_id_);
}

FileSystemAccessLockManager::FileSystemAccessLockManager(
    base::PassKey<FileSystemAccessManagerImpl> /*pass_key*/)
    : base::RefCountedDeleteOnSequence<FileSystemAccessLockManager>(
          base::SequencedTaskRunner::GetCurrentDefault()) {}

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

  base::FilePath cur_component_path;
  std::vector<base::FilePath::StringType> components =
      url.path().GetComponents();
  for (size_t i = 0; i < components.size(); ++i) {
    cur_component_path = i == 0 ? base::FilePath(components[0])
                                : cur_component_path.Append(components[i]);
    LockType component_lock_type =
        i == components.size() - 1 ? lock_type : ancestor_lock_type_;
    lock = lock->GetChild(cur_component_path);
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

void FileSystemAccessLockManager::TakeLock(
    const GlobalRenderFrameHostId& frame_id,
    const storage::FileSystemURL& url,
    LockType lock_type,
    TakeLockCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // GetOrCreateRootLock should always succeed.
  Lock* lock = GetOrCreateRootLock(RootLocator::FromFileSystemURL(url));
  CHECK(lock);

  // Attempt to take a lock on all components in the path.
  base::FilePath cur_component_path;
  std::vector<base::FilePath::StringType> components =
      url.path().GetComponents();
  for (size_t i = 0; i < components.size(); ++i) {
    cur_component_path = i == 0 ? base::FilePath(components[0])
                                : cur_component_path.Append(components[i]);
    // Take `lock_type` on the base and ancestor locks on the ancestors.
    LockType component_lock_type =
        i == components.size() - 1 ? lock_type : ancestor_lock_type_;
    lock = lock->GetOrCreateChild(cur_component_path, component_lock_type);
    if (!lock) {
      // Couldn't take lock due to contention with a lock in `url`'s path held
      // by an active page.
      //
      // No locks have been created yet, so no cleanup is necessary.
      std::move(callback).Run(nullptr);
      return;
    }
  }

  // If the lock is pending, store the callback so we can run it after eviction.
  if (lock->InPendingSubtree()) {
    lock->StorePendingCallback(std::move(callback), frame_id);
    return;
  }

  // If its not pending, then run the callback immediately.
  std::move(callback).Run(lock->CreateLockHandle(frame_id));
}

void FileSystemAccessLockManager::ReleaseRoot(const RootLocator& root_locator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // May destroy `this` if `FileSystemAccessManagerImpl` no longer holds a
  // `scoped_refptr` to `this`, and this is the last root lock.
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
    root_lock = new RootLock(this, root_locator);
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

base::WeakPtr<FileSystemAccessLockManager>
FileSystemAccessLockManager::GetWeakPtrForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

}  // namespace content
