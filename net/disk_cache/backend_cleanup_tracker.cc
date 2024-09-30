// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Internal helper used to sequence cleanup and reuse of cache directories
// among different objects.

#include "net/disk_cache/backend_cleanup_tracker.h"

#include <unordered_map>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"

namespace disk_cache {

namespace {

using TrackerMap =
    std::unordered_map<base::FilePath,
                       raw_ptr<BackendCleanupTracker, CtnExperimental>>;
struct AllBackendCleanupTrackers {
  TrackerMap map;

  // Since clients can potentially call CreateCacheBackend from multiple
  // threads, we need to lock the map keeping track of cleanup trackers
  // for these backends. Our overall strategy is to have TryCreate
  // acts as an arbitrator --- whatever thread grabs one, gets to operate
  // on the tracker freely until it gets destroyed.
  base::Lock lock;
};

static base::LazyInstance<AllBackendCleanupTrackers>::Leaky g_all_trackers;

}  // namespace.

// static
scoped_refptr<BackendCleanupTracker> BackendCleanupTracker::TryCreate(
    const base::FilePath& path,
    base::OnceClosure retry_closure) {
  AllBackendCleanupTrackers* all_trackers = g_all_trackers.Pointer();
  base::AutoLock lock(all_trackers->lock);

  std::pair<TrackerMap::iterator, bool> insert_result =
      all_trackers->map.insert(
          std::pair<base::FilePath, BackendCleanupTracker*>(path, nullptr));
  if (insert_result.second) {
    auto tracker = base::WrapRefCounted(new BackendCleanupTracker(path));
    insert_result.first->second = tracker.get();
    return tracker;
  } else {
    insert_result.first->second->AddPostCleanupCallbackImpl(
        std::move(retry_closure));
    return nullptr;
  }
}

void BackendCleanupTracker::AddPostCleanupCallback(base::OnceClosure cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(seq_checker_);
  // Despite the sequencing requirement we need to grab the table lock since
  // this may otherwise race against TryMakeContext.
  base::AutoLock lock(g_all_trackers.Get().lock);
  AddPostCleanupCallbackImpl(std::move(cb));
}

void BackendCleanupTracker::AddPostCleanupCallbackImpl(base::OnceClosure cb) {
  post_cleanup_cbs_.emplace_back(base::SequencedTaskRunner::GetCurrentDefault(),
                                 std::move(cb));
}

BackendCleanupTracker::BackendCleanupTracker(const base::FilePath& path)
    : path_(path) {}

BackendCleanupTracker::~BackendCleanupTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(seq_checker_);

  {
    AllBackendCleanupTrackers* all_trackers = g_all_trackers.Pointer();
    base::AutoLock lock(all_trackers->lock);
    int rv = all_trackers->map.erase(path_);
    DCHECK_EQ(1, rv);
  }

  while (!post_cleanup_cbs_.empty()) {
    post_cleanup_cbs_.back().first->PostTask(
        FROM_HERE, std::move(post_cleanup_cbs_.back().second));
    post_cleanup_cbs_.pop_back();
  }
}

}  // namespace disk_cache
