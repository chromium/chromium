// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_DATA_CONDITIONAL_CACHE_DELETION_HELPER_H_
#define CONTENT_BROWSER_BROWSING_DATA_CONDITIONAL_CACHE_DELETION_HELPER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/common/content_export.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "url/gurl.h"

namespace disk_cache {
class Entry;
}

namespace content {

// Helper to remove http/code cache data from a StoragePartition.
class CONTENT_EXPORT ConditionalCacheDeletionHelper {
 public:
  // Creates a helper to delete |cache| entries that match the |condition|.
  ConditionalCacheDeletionHelper(
      disk_cache::Backend* cache,
      base::RepeatingCallback<bool(const disk_cache::Entry*)> condition);

  // A convenience method to create a condition matching cache entries whose
  // last modified time is between |begin_time| (inclusively), |end_time|
  // (exclusively) and whose URL obtained by passing the key to the
  // |get_url_from_key| is matched by the |url_predicate|. The
  // |get_url_from_key| method is useful when the entries are not keyed by the
  // resource url alone. For ex: using two keys for site isolation.
  static base::RepeatingCallback<bool(const disk_cache::Entry*)>
  CreateURLAndTimeCondition(
      base::RepeatingCallback<bool(const GURL&)> url_predicate,
      base::RepeatingCallback<std::string(const std::string&)> get_url_from_key,
      base::Time begin_time,
      base::Time end_time);

  // Deletes the cache entries according to the specified condition. Destroys
  // this instance of ConditionalCacheDeletionHelper when finished.
  //
  // The return value is a net error code. If this method returns
  // ERR_IO_PENDING, the |completion_callback| will be invoked when the
  // operation completes.
  int DeleteAndDestroySelfWhenFinished(
      net::CompletionOnceCallback completion_callback);

 private:
  friend class base::DeleteHelper<ConditionalCacheDeletionHelper>;
  ~ConditionalCacheDeletionHelper();

  void IterateOverEntries(disk_cache::EntryResult result);

  disk_cache::Backend* cache_;
  const base::RepeatingCallback<bool(const disk_cache::Entry*)> condition_;

  net::CompletionOnceCallback completion_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<disk_cache::Backend::Iterator> iterator_;
  disk_cache::Entry* previous_entry_;

  DISALLOW_COPY_AND_ASSIGN(ConditionalCacheDeletionHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_DATA_CONDITIONAL_CACHE_DELETION_HELPER_H_
