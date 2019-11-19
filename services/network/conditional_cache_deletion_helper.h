// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CONDITIONAL_CACHE_DELETION_HELPER_H_
#define SERVICES_NETWORK_CONDITIONAL_CACHE_DELETION_HELPER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "url/gurl.h"

namespace disk_cache {
class Entry;
}

namespace network {

// Helper to remove HTTP cache data.
class ConditionalCacheDeletionHelper {
 public:
  // Creates a helper to delete |cache| entries whose last modified time is
  // between |begin_time| (inclusively), |end_time| (exclusively) and whose URL
  // is matched by the |url_matcher|. Note that |begin_time| and |end_time| can
  // be null to indicate unbounded time interval in their respective direction.
  // Starts the deletion and calls |completion_callback| when done.
  static std::unique_ptr<ConditionalCacheDeletionHelper> CreateAndStart(
      disk_cache::Backend* cache,
      const base::RepeatingCallback<bool(const GURL&)>& url_matcher,
      const base::Time& begin_time,
      const base::Time& end_time,
      base::OnceClosure completion_callback);

  ~ConditionalCacheDeletionHelper();

 private:
  ConditionalCacheDeletionHelper(
      const base::RepeatingCallback<bool(const disk_cache::Entry*)>& condition,
      base::OnceClosure completion_callback,
      std::unique_ptr<disk_cache::Backend::Iterator> iterator);

  void IterateOverEntries(disk_cache::EntryResult result);

  void NotifyCompletion();

  const base::RepeatingCallback<bool(const disk_cache::Entry*)> condition_;

  base::OnceClosure completion_callback_;

  std::unique_ptr<disk_cache::Backend::Iterator> iterator_;
  disk_cache::Entry* previous_entry_ = nullptr;

  base::WeakPtrFactory<ConditionalCacheDeletionHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ConditionalCacheDeletionHelper);
};

}  // namespace network

#endif  // SERVICES_NETWORK_CONDITIONAL_CACHE_DELETION_HELPER_H_
