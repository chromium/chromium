// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CROSS_SEQUENCE_CROSS_SEQUENCE_CACHE_STORAGE_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CROSS_SEQUENCE_CROSS_SEQUENCE_CACHE_STORAGE_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage_manager.h"

namespace content {

class CacheStorageContextWithManager;

// A CacheStorage implementation that can be used from one sequence to access
// a real CacheStorage executing on a different sequence.  The
// CrossSequenceCacheStorageManager constructs instances of this class in
// OpenCacheStorage().  Each CrossSequenceCacheStorage is ref-counted and
// the existence of a CacheStorageHandle will cause the instance to hold a
// scoped_refptr to itself.  Once all the handles have been dropped the
// self-reference is also dropped allowing the CrossSequenceCacheStorage to
// be destroyed.
class CrossSequenceCacheStorage
    : public CacheStorage,
      public base::RefCounted<CrossSequenceCacheStorage> {
 public:
  CrossSequenceCacheStorage(
      const url::Origin& origin,
      CacheStorageOwner owner,
      scoped_refptr<base::SequencedTaskRunner> target_task_runner,
      scoped_refptr<CacheStorageContextWithManager> context);

  // CacheStorage
  CacheStorageHandle CreateHandle() override;
  void AddHandleRef() override;
  void DropHandleRef() override;
  void OpenCache(const std::string& cache_name,
                 int64_t trace_id,
                 CacheAndErrorCallback callback) override;
  void HasCache(const std::string& cache_name,
                int64_t trace_id,
                BoolAndErrorCallback callback) override;
  void DoomCache(const std::string& cache_name,
                 int64_t trace_id,
                 ErrorCallback callback) override;
  void EnumerateCaches(int64_t trace_id,
                       EnumerateCachesCallback callback) override;
  void MatchCache(const std::string& cache_name,
                  blink::mojom::FetchAPIRequestPtr request,
                  blink::mojom::CacheQueryOptionsPtr match_options,
                  CacheStorageSchedulerPriority priority,
                  int64_t trace_id,
                  CacheStorageCache::ResponseCallback callback) override;
  void MatchAllCaches(blink::mojom::FetchAPIRequestPtr request,
                      blink::mojom::CacheQueryOptionsPtr match_options,
                      CacheStorageSchedulerPriority priority,
                      int64_t trace_id,
                      CacheStorageCache::ResponseCallback callback) override;
  void WriteToCache(const std::string& cache_name,
                    blink::mojom::FetchAPIRequestPtr request,
                    blink::mojom::FetchAPIResponsePtr response,
                    int64_t trace_id,
                    ErrorCallback callback) override;

 private:
  friend class base::RefCounted<CrossSequenceCacheStorage>;
  ~CrossSequenceCacheStorage() override;

  const scoped_refptr<base::SequencedTaskRunner> target_task_runner_;

  // The |inner_| object is SequenceBound<> to the target sequence used by the
  // real CacheStorage.
  class Inner;
  base::SequenceBound<Inner> inner_;

  // |self_ref_| holds a reference to the current |this| as long as
  // |handle_ref_count_| is greater than zero.  The |handle_ref_count_| is
  // incremented for every outstanding CacheStorageCacheHandle.
  scoped_refptr<CrossSequenceCacheStorage> self_ref_;
  int handle_ref_count_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CrossSequenceCacheStorage> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(CrossSequenceCacheStorage);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CROSS_SEQUENCE_CROSS_SEQUENCE_CACHE_STORAGE_H_
