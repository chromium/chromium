// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_DATA_STORAGE_PARTITION_HTTP_CACHE_DATA_REMOVER_H_
#define CONTENT_BROWSER_BROWSING_DATA_STORAGE_PARTITION_HTTP_CACHE_DATA_REMOVER_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/time/time.h"
#include "net/base/completion_callback.h"
#include "url/gurl.h"

namespace disk_cache {
class Backend;
}

namespace net {
class URLRequestContextGetter;
}

namespace content {

class StoragePartition;

// Helper to remove http cache data from a StoragePartition.
class StoragePartitionHttpCacheDataRemover {
 public:
  // Creates a StoragePartitionHttpCacheDataRemover that deletes cache entries
  // in the time range between |delete_begin| (inclusively) and |delete_end|
  // (exclusively).
  static StoragePartitionHttpCacheDataRemover* CreateForRange(
      content::StoragePartition* storage_partition,
      base::Time delete_begin,
      base::Time delete_end);

  // Similar to CreateForRange(), but only deletes URLs that are matched by
  // |url_predicate|. Note that the deletion with URL filtering is not built in
  // to the cache interface and might be slower.
  static StoragePartitionHttpCacheDataRemover* CreateForURLsAndRange(
      content::StoragePartition* storage_partition,
      const base::Callback<bool(const GURL&)>& url_predicate,
      base::Time delete_begin,
      base::Time delete_end);

  // Calls |done_callback| upon completion and also destroys itself.
  void Remove(base::OnceClosure done_callback);

 private:
  enum CacheState {
    NONE,
    CREATE_MAIN,
    CREATE_MEDIA,
    DELETE_MAIN,
    DELETE_MEDIA,
    DONE
  };

  StoragePartitionHttpCacheDataRemover(
      base::Callback<bool(const GURL&)> url_predicate,
      base::Time delete_begin,
      base::Time delete_end,
      net::URLRequestContextGetter* main_context_getter,
      net::URLRequestContextGetter* media_context_getter);

  // StoragePartitionHttpCacheDataRemover deletes itself (using DeleteHelper)
  // and is not supposed to be deleted by other objects so make destructor
  // private and DeleteHelper a friend.
  friend class base::DeleteHelper<StoragePartitionHttpCacheDataRemover>;

  ~StoragePartitionHttpCacheDataRemover();

  void ClearHttpCacheOnIOThread();
  void ClearedHttpCache();
  // Performs the actual work to delete or count the cache.
  void DoClearCache(int rv);

  base::Callback<bool(const GURL&)> url_predicate_;
  const base::Time delete_begin_;
  const base::Time delete_end_;

  const scoped_refptr<net::URLRequestContextGetter> main_context_getter_;
  const scoped_refptr<net::URLRequestContextGetter> media_context_getter_;

  base::OnceClosure done_callback_;

  // IO.
  int next_cache_state_;
  disk_cache::Backend* cache_;

  DISALLOW_COPY_AND_ASSIGN(StoragePartitionHttpCacheDataRemover);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_DATA_STORAGE_PARTITION_HTTP_CACHE_DATA_REMOVER_H_
