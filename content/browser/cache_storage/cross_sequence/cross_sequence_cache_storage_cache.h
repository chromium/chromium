// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CROSS_SEQUENCE_CROSS_SEQUENCE_CACHE_STORAGE_CACHE_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CROSS_SEQUENCE_CROSS_SEQUENCE_CACHE_STORAGE_CACHE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/cache_storage/cache_storage_cache.h"

namespace content {

// A CacheStorageCache implementation that can be used from one sequence to
// access a real CacheStorageCache executing on a different sequence.
// CrossSequenceCacheStorage objects construct instances of this class in
// OpenCache().  Each CrossSequenceCacheStorageCache is ref-counted and
// the existence of a CacheStorageCacheHandle will cause the instance to hold a
// scoped_refptr to itself.  Once all the handles have been dropped the
// self-reference is also dropped allowing the CrossSequenceCacheStorage to
// be destroyed.
class CrossSequenceCacheStorageCache
    : public CacheStorageCache,
      public base::RefCounted<CrossSequenceCacheStorageCache> {
 public:
  // Created on the outer source sequence without any reference to a real
  // cache yet.  SetHandleOnTaskRunner() must be called before any cache
  // methods are invoked.
  explicit CrossSequenceCacheStorageCache(
      scoped_refptr<base::SequencedTaskRunner> target_task_runner);

  // Called on the inner cache's sequence to set the real cache's handle.
  void SetHandleOnTaskRunner(CacheStorageCacheHandle handle);

  // CacheStorageCache
  CacheStorageCacheHandle CreateHandle() override;
  void AddHandleRef() override;
  void DropHandleRef() override;
  bool IsUnreferenced() const override;
  void Match(blink::mojom::FetchAPIRequestPtr request,
             blink::mojom::CacheQueryOptionsPtr match_options,
             CacheStorageSchedulerPriority priority,
             int64_t trace_id,
             ResponseCallback callback) override;

  void MatchAll(blink::mojom::FetchAPIRequestPtr request,
                blink::mojom::CacheQueryOptionsPtr match_options,
                int64_t trace_id,
                ResponsesCallback callback) override;

  void WriteSideData(ErrorCallback callback,
                     const GURL& url,
                     base::Time expected_response_time,
                     int64_t trace_id,
                     scoped_refptr<net::IOBuffer> buffer,
                     int buf_len) override;

  void BatchOperation(std::vector<blink::mojom::BatchOperationPtr> operations,
                      int64_t trace_id,
                      VerboseErrorCallback callback,
                      BadMessageCallback bad_message_callback) override;

  void Keys(blink::mojom::FetchAPIRequestPtr request,
            blink::mojom::CacheQueryOptionsPtr options,
            int64_t trace_id,
            RequestsCallback callback) override;

  void Put(blink::mojom::FetchAPIRequestPtr request,
           blink::mojom::FetchAPIResponsePtr response,
           int64_t trace_id,
           ErrorCallback callback) override;

  void GetAllMatchedEntries(blink::mojom::FetchAPIRequestPtr request,
                            blink::mojom::CacheQueryOptionsPtr match_options,
                            int64_t trace_id,
                            CacheEntriesCallback callback) override;

  InitState GetInitState() const override;

 private:
  friend class base::RefCounted<CrossSequenceCacheStorageCache>;
  ~CrossSequenceCacheStorageCache() override;

  // The |inner_| object is SequenceBound<> to the target sequence used by the
  // real CacheStorageCache.
  class Inner;
  base::SequenceBound<Inner> inner_;

  // |self_ref_| holds a reference to the current |this| as long as
  // |handle_ref_count_| is greater than zero.  The |handle_ref_count_| is
  // incremented for every outstanding CacheStorageCacheHandle.
  scoped_refptr<CrossSequenceCacheStorageCache> self_ref_;
  int handle_ref_count_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CrossSequenceCacheStorageCache> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(CrossSequenceCacheStorageCache);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CROSS_SEQUENCE_CROSS_SEQUENCE_CACHE_STORAGE_CACHE_H_
