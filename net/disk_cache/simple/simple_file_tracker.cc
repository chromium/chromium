// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_file_tracker.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/lock.h"
#include "net/disk_cache/simple/simple_histogram_enums.h"
#include "net/disk_cache/simple/simple_synchronous_entry.h"

namespace disk_cache {

namespace {

void RecordFileDescripterLimiterOp(FileDescriptorLimiterOp op) {
  UMA_HISTOGRAM_ENUMERATION("SimpleCache.FileDescriptorLimiterAction", op,
                            FD_LIMIT_OP_MAX);
}

}  // namespace

SimpleFileTracker::SimpleFileTracker(int file_limit)
    : file_limit_(file_limit) {}

SimpleFileTracker::~SimpleFileTracker() {
  DCHECK(lru_.empty());
  DCHECK(tracked_files_.empty());
}

void SimpleFileTracker::Register(const SimpleSynchronousEntry* owner,
                                 SubFile subfile,
                                 std::unique_ptr<base::File> file) {
  DCHECK(file->IsValid());
  std::vector<std::unique_ptr<base::File>> files_to_close;

  {
    base::AutoLock hold_lock(lock_);

    // Make sure the list of everything with given hash exists.
    auto insert_status = tracked_files_.insert(
        std::make_pair(owner->entry_file_key().entry_hash,
                       std::vector<std::unique_ptr<TrackedFiles>>()));

    std::vector<std::unique_ptr<TrackedFiles>>& candidates =
        insert_status.first->second;

    // See if entry for |owner| already exists, if not append.
    TrackedFiles* owners_files = nullptr;
    for (const std::unique_ptr<TrackedFiles>& candidate : candidates) {
      if (candidate->owner == owner) {
        owners_files = candidate.get();
        break;
      }
    }

    if (!owners_files) {
      candidates.emplace_back(new TrackedFiles());
      owners_files = candidates.back().get();
      owners_files->owner = owner;
      owners_files->key = owner->entry_file_key();
    }

    EnsureInFrontOfLRU(owners_files);

    int file_index = static_cast<int>(subfile);
    DCHECK_EQ(TrackedFiles::TF_NO_REGISTRATION,
              owners_files->state[file_index]);
    owners_files->files[file_index] = std::move(file);
    owners_files->state[file_index] = TrackedFiles::TF_REGISTERED;
    ++open_files_;
    CloseFilesIfTooManyOpen(&files_to_close);
  }
}

SimpleFileTracker::FileHandle SimpleFileTracker::Acquire(
    const SimpleSynchronousEntry* owner,
    SubFile subfile) {
  std::vector<std::unique_ptr<base::File>> files_to_close;

  {
    base::AutoLock hold_lock(lock_);
    TrackedFiles* owners_files = Find(owner);
    int file_index = static_cast<int>(subfile);

    DCHECK_EQ(TrackedFiles::TF_REGISTERED, owners_files->state[file_index]);
    owners_files->state[file_index] = TrackedFiles::TF_ACQUIRED;
    EnsureInFrontOfLRU(owners_files);

    // Check to see if we have to reopen the file. That might push us over the
    // fd limit.  CloseFilesIfTooManyOpen will not close anything in
    // |*owners_files| since it's already in the the TF_ACQUIRED state.
    if (owners_files->files[file_index] == nullptr) {
      ReopenFile(owners_files, subfile);
      CloseFilesIfTooManyOpen(&files_to_close);
    }

    return FileHandle(this, owner, subfile,
                      owners_files->files[file_index].get());
  }
}

SimpleFileTracker::TrackedFiles::TrackedFiles() : in_lru(false) {
  std::fill(state, state + kSimpleEntryTotalFileCount, TF_NO_REGISTRATION);
}

SimpleFileTracker::TrackedFiles::~TrackedFiles() = default;

bool SimpleFileTracker::TrackedFiles::Empty() const {
  for (State s : state)
    if (s != TF_NO_REGISTRATION)
      return false;
  return true;
}

bool SimpleFileTracker::TrackedFiles::HasOpenFiles() const {
  for (const std::unique_ptr<base::File>& file : files)
    if (file != nullptr)
      return true;
  return false;
}

void SimpleFileTracker::Release(const SimpleSynchronousEntry* owner,
                                SubFile subfile) {
  std::vector<std::unique_ptr<base::File>> files_to_close;

  {
    base::AutoLock hold_lock(lock_);
    TrackedFiles* owners_files = Find(owner);
    int file_index = static_cast<int>(subfile);

    DCHECK(owners_files->state[file_index] == TrackedFiles::TF_ACQUIRED ||
           owners_files->state[file_index] ==
               TrackedFiles::TF_ACQUIRED_PENDING_CLOSE);

    // Prepare to executed deferred close, if any.
    if (owners_files->state[file_index] ==
        TrackedFiles::TF_ACQUIRED_PENDING_CLOSE) {
      files_to_close.push_back(PrepareClose(owners_files, file_index));
    } else {
      owners_files->state[file_index] = TrackedFiles::TF_REGISTERED;
    }

    // It's possible that we were over limit and couldn't do much about it
    // since everything was lent out, so now may be the time to close extra
    // stuff.
    CloseFilesIfTooManyOpen(&files_to_close);
  }
}

void SimpleFileTracker::Close(const SimpleSynchronousEntry* owner,
                              SubFile subfile) {
  std::unique_ptr<base::File> file_to_close;

  {
    base::AutoLock hold_lock(lock_);
    TrackedFiles* owners_files = Find(owner);
    int file_index = static_cast<int>(subfile);

    DCHECK(owners_files->state[file_index] == TrackedFiles::TF_ACQUIRED ||
           owners_files->state[file_index] == TrackedFiles::TF_REGISTERED);

    if (owners_files->state[file_index] == TrackedFiles::TF_ACQUIRED) {
      // The FD is currently acquired, so we can't clean up the TrackedFiles,
      // just yet; even if this is the last close, so delay the close until it
      // gets released.
      owners_files->state[file_index] = TrackedFiles::TF_ACQUIRED_PENDING_CLOSE;
    } else {
      file_to_close = PrepareClose(owners_files, file_index);
    }
  }
}

void SimpleFileTracker::Doom(const SimpleSynchronousEntry* owner,
                             EntryFileKey* key) {
  base::AutoLock hold_lock(lock_);
  auto iter = tracked_files_.find(key->entry_hash);
  DCHECK(iter != tracked_files_.end());

  uint64_t max_doom_gen = 0;
  for (const std::unique_ptr<TrackedFiles>& file_with_same_hash :
       iter->second) {
    max_doom_gen =
        std::max(max_doom_gen, file_with_same_hash->key.doom_generation);
  }

  // It would take >502 years to doom the same hash enough times (at 10^9 dooms
  // per second) to wrap the 64 bit counter. Still, if it does wrap around,
  // there is a security risk since we could confuse different keys.
  CHECK_NE(max_doom_gen, std::numeric_limits<uint64_t>::max());
  uint64_t new_doom_gen = max_doom_gen + 1;

  // Update external key.
  key->doom_generation = new_doom_gen;

  // Update our own.
  for (const std::unique_ptr<TrackedFiles>& file_with_same_hash :
       iter->second) {
    if (file_with_same_hash->owner == owner)
      file_with_same_hash->key.doom_generation = new_doom_gen;
  }
}

bool SimpleFileTracker::IsEmptyForTesting() {
  base::AutoLock hold_lock(lock_);
  return tracked_files_.empty() && lru_.empty();
}

SimpleFileTracker::TrackedFiles* SimpleFileTracker::Find(
    const SimpleSynchronousEntry* owner) {
  auto candidates = tracked_files_.find(owner->entry_file_key().entry_hash);
  DCHECK(candidates != tracked_files_.end());
  for (const auto& candidate : candidates->second) {
    if (candidate->owner == owner) {
      return candidate.get();
    }
  }
  LOG(DFATAL) << "SimpleFileTracker operation on non-found entry";
  return nullptr;
}

std::unique_ptr<base::File> SimpleFileTracker::PrepareClose(
    TrackedFiles* owners_files,
    int file_index) {
  std::unique_ptr<base::File> file_out =
      std::move(owners_files->files[file_index]);
  owners_files->state[file_index] = TrackedFiles::TF_NO_REGISTRATION;
  if (owners_files->Empty()) {
    auto iter = tracked_files_.find(owners_files->key.entry_hash);
    for (auto i = iter->second.begin(); i != iter->second.end(); ++i) {
      if ((*i).get() == owners_files) {
        if (owners_files->in_lru)
          lru_.erase(owners_files->position_in_lru);
        iter->second.erase(i);
        break;
      }
    }
    if (iter->second.empty())
      tracked_files_.erase(iter);
  }
  if (file_out != nullptr)
    --open_files_;
  return file_out;
}

void SimpleFileTracker::CloseFilesIfTooManyOpen(
    std::vector<std::unique_ptr<base::File>>* files_to_close) {
  auto i = lru_.end();
  while (open_files_ > file_limit_ && i != lru_.begin()) {
    --i;  // Point to the actual entry.
    TrackedFiles* tracked_files = *i;
    DCHECK(tracked_files->in_lru);
    for (int j = 0; j < kSimpleEntryTotalFileCount; ++j) {
      if (tracked_files->state[j] == TrackedFiles::TF_REGISTERED &&
          tracked_files->files[j] != nullptr) {
        files_to_close->push_back(std::move(tracked_files->files[j]));
        --open_files_;
        RecordFileDescripterLimiterOp(FD_LIMIT_CLOSE_FILE);
      }
    }

    if (!tracked_files->HasOpenFiles()) {
      // If there is nothing here that can possibly be closed, remove this from
      // LRU for now so we don't have to rescan it next time we are here. If the
      // files get re-opened (in Acquire), it will get added back in.
      DCHECK_EQ(*tracked_files->position_in_lru, tracked_files);
      DCHECK(i == tracked_files->position_in_lru);
      // Note that we're erasing at i, which would make it invalid, so go back
      // one element ahead to we can decrement from that on next iteration.
      ++i;
      lru_.erase(tracked_files->position_in_lru);
      tracked_files->in_lru = false;
    }
  }
}

void SimpleFileTracker::ReopenFile(TrackedFiles* owners_files,
                                   SubFile subfile) {
  int file_index = static_cast<int>(subfile);
  DCHECK(owners_files->files[file_index] == nullptr);
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_WRITE | base::File::FLAG_SHARE_DELETE;
  base::FilePath file_path =
      owners_files->owner->GetFilenameForSubfile(subfile);
  owners_files->files[file_index] =
      std::make_unique<base::File>(file_path, flags);
  if (owners_files->files[file_index]->IsValid()) {
    RecordFileDescripterLimiterOp(FD_LIMIT_REOPEN_FILE);

    ++open_files_;
  } else {
    owners_files->files[file_index] = nullptr;
    RecordFileDescripterLimiterOp(FD_LIMIT_FAIL_REOPEN_FILE);
  }
}

void SimpleFileTracker::EnsureInFrontOfLRU(TrackedFiles* owners_files) {
  if (!owners_files->in_lru) {
    lru_.push_front(owners_files);
    owners_files->position_in_lru = lru_.begin();
    owners_files->in_lru = true;
  } else if (owners_files->position_in_lru != lru_.begin()) {
    lru_.splice(lru_.begin(), lru_, owners_files->position_in_lru);
  }
  DCHECK_EQ(*owners_files->position_in_lru, owners_files);
}

SimpleFileTracker::FileHandle::FileHandle() = default;

SimpleFileTracker::FileHandle::FileHandle(SimpleFileTracker* file_tracker,
                                          const SimpleSynchronousEntry* entry,
                                          SimpleFileTracker::SubFile subfile,
                                          base::File* file)
    : file_tracker_(file_tracker),
      entry_(entry),
      subfile_(subfile),
      file_(file) {}

SimpleFileTracker::FileHandle::FileHandle(FileHandle&& other) {
  *this = std::move(other);
}

SimpleFileTracker::FileHandle::~FileHandle() {
  if (entry_)
    file_tracker_->Release(entry_, subfile_);
}

SimpleFileTracker::FileHandle& SimpleFileTracker::FileHandle::operator=(
    FileHandle&& other) {
  file_tracker_ = other.file_tracker_;
  entry_ = other.entry_;
  subfile_ = other.subfile_;
  file_ = other.file_;
  other.file_tracker_ = nullptr;
  other.entry_ = nullptr;
  other.file_ = nullptr;
  return *this;
}

base::File* SimpleFileTracker::FileHandle::operator->() const {
  return file_;
}

base::File* SimpleFileTracker::FileHandle::get() const {
  return file_;
}

bool SimpleFileTracker::FileHandle::IsOK() const {
  return file_ && file_->IsValid();
}

}  // namespace disk_cache
