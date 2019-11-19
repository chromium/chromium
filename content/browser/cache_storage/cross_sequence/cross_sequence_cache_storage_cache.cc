// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cross_sequence/cross_sequence_cache_storage_cache.h"

#include "content/browser/cache_storage/cache_storage_histogram_utils.h"
#include "content/browser/cache_storage/cross_sequence/cross_sequence_utils.h"

namespace content {

// The Inner class is SequenceBound<> to the real target CacheStorageCache
// sequence by the outer CrossSequenceCacheStorageCache.  All CacheStorageCache
// operations are proxied to the Inner on the correct sequence via the Post()
// method.  The outer storage is responsible for wrapping any callbacks in
// order to post on the outer's original sequence.
class CrossSequenceCacheStorageCache::Inner {
 public:
  void SetHandle(CacheStorageCacheHandle handle) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!handle_.value());
    handle_ = std::move(handle);
  }

  void Match(blink::mojom::FetchAPIRequestPtr request,
             blink::mojom::CacheQueryOptionsPtr match_options,
             CacheStorageSchedulerPriority priority,
             int64_t trace_id,
             ResponseCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!handle_.value()) {
      std::move(callback).Run(
          MakeErrorStorage(ErrorStorageType::kStorageHandleNull), nullptr);
      return;
    }
    handle_.value()->Match(std::move(request), std::move(match_options),
                           priority, trace_id, std::move(callback));
  }

  void MatchAll(blink::mojom::FetchAPIRequestPtr request,
                blink::mojom::CacheQueryOptionsPtr match_options,
                int64_t trace_id,
                ResponsesCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!handle_.value()) {
      std::move(callback).Run(
          MakeErrorStorage(ErrorStorageType::kStorageHandleNull),
          std::vector<blink::mojom::FetchAPIResponsePtr>());
      return;
    }
    handle_.value()->MatchAll(std::move(request), std::move(match_options),
                              trace_id, std::move(callback));
  }

  void WriteSideData(ErrorCallback callback,
                     const GURL& url,
                     base::Time expected_response_time,
                     int64_t trace_id,
                     scoped_refptr<net::IOBuffer> buffer,
                     int buf_len) {
    if (!handle_.value()) {
      std::move(callback).Run(
          MakeErrorStorage(ErrorStorageType::kStorageHandleNull));
      return;
    }
    handle_.value()->WriteSideData(std::move(callback), url,
                                   expected_response_time, trace_id,
                                   std::move(buffer), buf_len);
  }

  void BatchOperation(std::vector<blink::mojom::BatchOperationPtr> operations,
                      int64_t trace_id,
                      VerboseErrorCallback callback,
                      BadMessageCallback bad_message_callback) {
    if (!handle_.value()) {
      std::move(callback).Run(blink::mojom::CacheStorageVerboseError::New(
          MakeErrorStorage(ErrorStorageType::kStorageHandleNull), nullptr));
      return;
    }
    handle_.value()->BatchOperation(std::move(operations), trace_id,
                                    std::move(callback),
                                    std::move(bad_message_callback));
  }

  void Keys(blink::mojom::FetchAPIRequestPtr request,
            blink::mojom::CacheQueryOptionsPtr options,
            int64_t trace_id,
            RequestsCallback callback) {
    if (!handle_.value()) {
      std::move(callback).Run(
          MakeErrorStorage(ErrorStorageType::kStorageHandleNull), nullptr);
      return;
    }
    handle_.value()->Keys(std::move(request), std::move(options), trace_id,
                          std::move(callback));
  }

  void Put(blink::mojom::FetchAPIRequestPtr request,
           blink::mojom::FetchAPIResponsePtr response,
           int64_t trace_id,
           ErrorCallback callback) {
    if (!handle_.value()) {
      std::move(callback).Run(
          MakeErrorStorage(ErrorStorageType::kStorageHandleNull));
      return;
    }
    handle_.value()->Put(std::move(request), std::move(response), trace_id,
                         std::move(callback));
  }

  void GetAllMatchedEntries(blink::mojom::FetchAPIRequestPtr request,
                            blink::mojom::CacheQueryOptionsPtr match_options,
                            int64_t trace_id,
                            CacheEntriesCallback callback) {
    if (!handle_.value()) {
      std::move(callback).Run(
          MakeErrorStorage(ErrorStorageType::kStorageHandleNull),
          std::vector<CacheEntry>());
      return;
    }
    handle_.value()->GetAllMatchedEntries(std::move(request),
                                          std::move(match_options), trace_id,
                                          std::move(callback));
  }

 private:
  CacheStorageCacheHandle handle_;
  SEQUENCE_CHECKER(sequence_checker_);
};

CrossSequenceCacheStorageCache::CrossSequenceCacheStorageCache(
    scoped_refptr<base::SequencedTaskRunner> target_task_runner)
    : inner_(std::move(target_task_runner)) {}

void CrossSequenceCacheStorageCache::SetHandleOnTaskRunner(
    CacheStorageCacheHandle handle) {
  // Called on target sequence.
  DCHECK(handle.value());
  // Even though we are already on the correct sequence we still have to Post()
  // to the inner object.
  inner_.Post(FROM_HERE, &Inner::SetHandle, std::move(handle));
}

CacheStorageCacheHandle CrossSequenceCacheStorageCache::CreateHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return CacheStorageCacheHandle(weak_factory_.GetWeakPtr());
}

void CrossSequenceCacheStorageCache::AddHandleRef() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  handle_ref_count_ += 1;
  if (handle_ref_count_ == 1)
    self_ref_ = base::WrapRefCounted(this);
}

void CrossSequenceCacheStorageCache::DropHandleRef() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(handle_ref_count_, 0);
  handle_ref_count_ -= 1;
  if (handle_ref_count_ == 0)
    self_ref_.reset();
}

bool CrossSequenceCacheStorageCache::IsUnreferenced() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return handle_ref_count_ == 0;
}

void CrossSequenceCacheStorageCache::Match(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr match_options,
    CacheStorageSchedulerPriority priority,
    int64_t trace_id,
    ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_.Post(FROM_HERE, &Inner::Match, std::move(request),
              std::move(match_options), priority, trace_id,
              WrapCallbackForCurrentSequence(std::move(callback)));
}

void CrossSequenceCacheStorageCache::MatchAll(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr match_options,
    int64_t trace_id,
    ResponsesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_.Post(FROM_HERE, &Inner::MatchAll, std::move(request),
              std::move(match_options), trace_id,
              WrapCallbackForCurrentSequence(std::move(callback)));
}

void CrossSequenceCacheStorageCache::WriteSideData(
    ErrorCallback callback,
    const GURL& url,
    base::Time expected_response_time,
    int64_t trace_id,
    scoped_refptr<net::IOBuffer> buffer,
    int buf_len) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_.Post(FROM_HERE, &Inner::WriteSideData,
              WrapCallbackForCurrentSequence(std::move(callback)), url,
              expected_response_time, trace_id, std::move(buffer), buf_len);
}

void CrossSequenceCacheStorageCache::BatchOperation(
    std::vector<blink::mojom::BatchOperationPtr> operations,
    int64_t trace_id,
    VerboseErrorCallback callback,
    BadMessageCallback bad_message_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_.Post(FROM_HERE, &Inner::BatchOperation, std::move(operations),
              trace_id, WrapCallbackForCurrentSequence(std::move(callback)),
              WrapCallbackForCurrentSequence(std::move(bad_message_callback)));
}

void CrossSequenceCacheStorageCache::Keys(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr options,
    int64_t trace_id,
    RequestsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_.Post(FROM_HERE, &Inner::Keys, std::move(request), std::move(options),
              trace_id, WrapCallbackForCurrentSequence(std::move(callback)));
}

void CrossSequenceCacheStorageCache::Put(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::FetchAPIResponsePtr response,
    int64_t trace_id,
    ErrorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_.Post(FROM_HERE, &Inner::Put, std::move(request), std::move(response),
              trace_id, WrapCallbackForCurrentSequence(std::move(callback)));
}

void CrossSequenceCacheStorageCache::GetAllMatchedEntries(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr match_options,
    int64_t trace_id,
    CacheEntriesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_.Post(FROM_HERE, &Inner::GetAllMatchedEntries, std::move(request),
              std::move(match_options), trace_id,
              WrapCallbackForCurrentSequence(std::move(callback)));
}

CacheStorageCache::InitState CrossSequenceCacheStorageCache::GetInitState()
    const {
  // We don't really know when the real cache on the other sequence becomes
  // initialized.
  return InitState::Unknown;
}

CrossSequenceCacheStorageCache::~CrossSequenceCacheStorageCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace content
