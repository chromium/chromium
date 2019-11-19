// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/vfs_wrapper_fuchsia.h"

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "sql/vfs_wrapper.h"

namespace sql {

namespace {

// Singleton that stores locks state.
class FuchsiaFileLockManager {
 public:
  FuchsiaFileLockManager() = default;

  // Returns lock manager for the current process.
  static FuchsiaFileLockManager* Instance() {
    static base::NoDestructor<FuchsiaFileLockManager> lock_manager;
    return lock_manager.get();
  }

  // Return true if the file was locked successfully.
  bool Lock(const std::string& name) {
    base::AutoLock lock(lock_);

    if (locked_files_.find(name) != locked_files_.end()) {
      DLOG(WARNING) << "File " << name
                    << " is being used concurrently by multiple consumers.";
      return false;
    }

    locked_files_.insert(name);
    return true;
  }

  void Unlock(const std::string& name) {
    base::AutoLock lock(lock_);

    size_t removed = locked_files_.erase(name);
    DCHECK_EQ(removed, 1U);
  }

  bool IsLocked(const std::string& name) {
    base::AutoLock lock(lock_);
    return locked_files_.find(name) != locked_files_.end();
  }

 private:
  ~FuchsiaFileLockManager() = delete;

  base::Lock lock_;

  // Set of all currently locked files.
  base::flat_set<std::string> locked_files_ GUARDED_BY(lock_);
};

}  // namespace

int FuchsiaVfsLock(sqlite3_file* sqlite_file, int file_lock) {
  DCHECK(file_lock == SQLITE_LOCK_SHARED || file_lock == SQLITE_LOCK_RESERVED ||
         file_lock == SQLITE_LOCK_PENDING ||
         file_lock == SQLITE_LOCK_EXCLUSIVE);

  VfsFile* vfs_file = reinterpret_cast<VfsFile*>(sqlite_file);

  if (vfs_file->lock_level == SQLITE_LOCK_NONE) {
    if (!FuchsiaFileLockManager::Instance()->Lock(vfs_file->file_name))
      return SQLITE_BUSY;
  }

  vfs_file->lock_level = file_lock;

  return SQLITE_OK;
}

int FuchsiaVfsUnlock(sqlite3_file* sqlite_file, int file_lock) {
  VfsFile* vfs_file = reinterpret_cast<VfsFile*>(sqlite_file);

  if (file_lock == SQLITE_LOCK_NONE) {
    if (vfs_file->lock_level != SQLITE_LOCK_NONE)
      FuchsiaFileLockManager::Instance()->Unlock(vfs_file->file_name);
  } else {
    // Keep the file locked for the shared lock.
    DCHECK(file_lock == SQLITE_LOCK_SHARED);
    DCHECK(FuchsiaFileLockManager::Instance()->IsLocked(vfs_file->file_name));
  }
  vfs_file->lock_level = file_lock;

  return SQLITE_OK;
}

int FuchsiaVfsCheckReservedLock(sqlite3_file* sqlite_file, int* result) {
  VfsFile* vfs_file = reinterpret_cast<VfsFile*>(sqlite_file);
  return vfs_file->lock_level;
}

}  // namespace sql
