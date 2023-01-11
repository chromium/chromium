// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_BACKEND_CLEANUP_TRACKER_H_
#define NET_DISK_CACHE_BACKEND_CLEANUP_TRACKER_H_

#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "net/base/net_export.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace disk_cache {

// Internal helper used to sequence cleanup and reuse of cache directories.
// One of these is created before each backend, and is kept alive till both
// the backend is destroyed and all of its work is done by its refcount,
// which keeps track of outstanding work. That refcount is expected to only be
// updated from the I/O thread or its equivalent.
class NET_EXPORT_PRIVATE BackendCleanupTracker
    : public base::RefCounted<BackendCleanupTracker> {
 public:
  // Either returns a fresh cleanup tracker for |path| if none exists, or
  // will eventually post |retry_closure| to the calling thread,
  // and return null.
  static scoped_refptr<BackendCleanupTracker> TryCreate(
      const base::FilePath& path,
      base::OnceClosure retry_closure);

  BackendCleanupTracker(const BackendCleanupTracker&) = delete;
  BackendCleanupTracker& operator=(const BackendCleanupTracker&) = delete;

  // Register a callback to be posted after all the work of associated
  // context is complete (which will result in destruction of this context).
  // Should only be called by owner, on its I/O-thread-like execution context,
  // and will in turn eventually post |cb| there.
  void AddPostCleanupCallback(base::OnceClosure cb);

 private:
  friend class base::RefCounted<BackendCleanupTracker>;
  explicit BackendCleanupTracker(const base::FilePath& path);
  ~BackendCleanupTracker();

  void AddPostCleanupCallbackImpl(base::OnceClosure cb);

  base::FilePath path_;

  // Since it's possible that a different thread may want to create a
  // cache for a reused path, we keep track of runners contexts of
  // post-cleanup callbacks.
  std::vector<
      std::pair<scoped_refptr<base::SequencedTaskRunner>, base::OnceClosure>>
      post_cleanup_cbs_;

  // We expect only TryMakeContext to be multithreaded, everything
  // else should be sequenced.
  SEQUENCE_CHECKER(seq_checker_);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BACKEND_CLEANUP_TRACKER_H_
