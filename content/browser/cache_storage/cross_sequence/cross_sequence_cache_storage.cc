// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cross_sequence/cross_sequence_cache_storage.h"

#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_histogram_utils.h"
#include "content/browser/cache_storage/cross_sequence/cross_sequence_cache_storage_cache.h"
#include "content/browser/cache_storage/cross_sequence/cross_sequence_utils.h"

namespace content {

// The Inner class is SequenceBound<> to the real target CacheStorage sequence
// by the outer CrossSequenceCacheStorage.  All CacheStorage operations are
// proxied to the Inner on the correct sequence via the Post() method.  The
// outer storage is responsible for wrapping any callbacks in order to post on
// the outer's original sequence.
class CrossSequenceCacheStorage::Inner {
 public:
  using OpenCacheAdapterCallback =
      base::OnceCallback<void(scoped_refptr<CrossSequenceCacheStorageCache>,
                              blink::mojom::CacheStorageError)>;

  Inner(const url::Origin& origin,
        CacheStorageOwner owner,
        scoped_refptr<CacheStorageContextWithManager> context)
      : handle_(context->CacheManager()->OpenCacheStorage(origin, owner)) {}

  void OpenCache(scoped_refptr<CrossSequenceCacheStorageCache> cache_wrapper,
                 const std::string& cache_name,
                 int64_t trace_id,
                 OpenCacheAdapterCallback adapter_callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!handle_.value()) {
      std::move(adapter_callback)
          .Run(std::move(cache_wrapper),
               MakeErrorStorage(ErrorStorageType::kStorageHandleNull));
      return;
    }

    // Open the cache and set the handle on the wrapper object provided.  The
    // wrapper will then be sent back to the source sequence to be exposed via
    // its own handle.
    handle_.value()->OpenCache(
        cache_name, trace_id,
        base::BindOnce(
            [](scoped_refptr<CrossSequenceCacheStorageCache> cache_wrapper,
               OpenCacheAdapterCallback adapter_callback,
               CacheStorageCacheHandle handle,
               blink::mojom::CacheStorageError error) {
              // Called on target TaskRunner.
              if (handle.value())
                cache_wrapper->SetHandleOnTaskRunner(std::move(handle));
              // Passing |cache_wrapper| back across the sequence boundary is
              // safe because we are guaranteed this is the only reference to
              // the object.
              std::move(adapter_callback).Run(std::move(cache_wrapper), error);
            },
            std::move(cache_wrapper), std::move(adapter_callback)));
  }

  void HasCache(const std::string& cache_name,
                int64_t trace_id,
                BoolAndErrorCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!handle_.value()) {
      std::move(callback).Run(
          false, MakeErrorStorage(ErrorStorageType::kStorageHandleNull));
      return;
    }
    handle_.value()->HasCache(cache_name, trace_id, std::move(callback));
  }

  void DoomCache(const std::string& cache_name,
                 int64_t trace_id,
                 ErrorCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!handle_.value()) {
      std::move(callback).Run(
          MakeErrorStorage(ErrorStorageType::kStorageHandleNull));
      return;
    }
    handle_.value()->DoomCache(cache_name, trace_id, std::move(callback));
  }

  void EnumerateCaches(int64_t trace_id, EnumerateCachesCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!handle_.value()) {
      std::move(callback).Run(std::vector<std::string>());
      return;
    }
    handle_.value()->EnumerateCaches(trace_id, std::move(callback));
  }

  void MatchCache(const std::string& cache_name,
                  blink::mojom::FetchAPIRequestPtr request,
                  blink::mojom::CacheQueryOptionsPtr match_options,
                  CacheStorageSchedulerPriority priority,
                  int64_t trace_id,
                  CacheStorageCache::ResponseCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!handle_.value()) {
      std::move(callback).Run(
          MakeErrorStorage(ErrorStorageType::kStorageHandleNull), nullptr);
      return;
    }
    handle_.value()->MatchCache(cache_name, std::move(request),
                                std::move(match_options), priority, trace_id,
                                std::move(callback));
  }

  void MatchAllCaches(blink::mojom::FetchAPIRequestPtr request,
                      blink::mojom::CacheQueryOptionsPtr match_options,
                      CacheStorageSchedulerPriority priority,
                      int64_t trace_id,
                      CacheStorageCache::ResponseCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!handle_.value()) {
      std::move(callback).Run(
          MakeErrorStorage(ErrorStorageType::kStorageHandleNull), nullptr);
      return;
    }
    handle_.value()->MatchAllCaches(std::move(request),
                                    std::move(match_options), priority,
                                    trace_id, std::move(callback));
  }

  void WriteToCache(const std::string& cache_name,
                    blink::mojom::FetchAPIRequestPtr request,
                    blink::mojom::FetchAPIResponsePtr response,
                    int64_t trace_id,
                    ErrorCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!handle_.value()) {
      std::move(callback).Run(
          MakeErrorStorage(ErrorStorageType::kStorageHandleNull));
      return;
    }
    handle_.value()->WriteToCache(cache_name, std::move(request),
                                  std::move(response), trace_id,
                                  std::move(callback));
  }

 private:
  const CacheStorageHandle handle_;
  SEQUENCE_CHECKER(sequence_checker_);
};

CrossSequenceCacheStorage::CrossSequenceCacheStorage(
    const url::Origin& origin,
    CacheStorageOwner owner,
    scoped_refptr<base::SequencedTaskRunner> target_task_runner,
    scoped_refptr<CacheStorageContextWithManager> context)
    : CacheStorage(origin),
      target_task_runner_(std::move(target_task_runner)),
      inner_(target_task_runner_,
             origin,
             std::move(owner),
             std::move(context)) {}

CacheStorageHandle CrossSequenceCacheStorage::CreateHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return CacheStorageHandle(weak_factory_.GetWeakPtr());
}

void CrossSequenceCacheStorage::AddHandleRef() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  handle_ref_count_ += 1;
  if (handle_ref_count_ == 1)
    self_ref_ = base::WrapRefCounted(this);
}

void CrossSequenceCacheStorage::DropHandleRef() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(handle_ref_count_, 0);
  handle_ref_count_ -= 1;
  if (handle_ref_count_ == 0)
    self_ref_.reset();
}

void CrossSequenceCacheStorage::OpenCache(const std::string& cache_name,
                                          int64_t trace_id,
                                          CacheAndErrorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Create our cross-sequence cache wrapper object first.  This will be sent
  // down to the target TaskRunner with our open request.  It must already
  // exist in order for the open request to set the real handle on it on the
  // target TaskRunner.  If an error occurs the wrapper object is thrown
  // away.
  auto cache_wrapper =
      base::MakeRefCounted<CrossSequenceCacheStorageCache>(target_task_runner_);

  // After the open request sets the real handle on the target TaskRunner our
  // cache wrapper will be passed back to this sequence.  We then create a
  // handle to the wrapper and pass that to the external callback.
  auto adapter_callback = base::BindOnce(
      [](CacheAndErrorCallback inner_callback,
         scoped_refptr<CrossSequenceCacheStorageCache> cache_wrapper,
         blink::mojom::CacheStorageError error) {
        if (error != blink::mojom::CacheStorageError::kSuccess) {
          // Don't create a handle to the wrapper if there was an error.
          // The |cache_wrapper| will be destroyed when it goes out of scope.
          std::move(inner_callback).Run(CacheStorageCacheHandle(), error);
          return;
        }
        // Called on source TaskRunner (thanks to callback wrapping below).
        // Note, CreateHandle() will cause the cache to remain strongly
        // referenced and survive even though |cache_wrapper| goes out of
        // scope.
        std::move(inner_callback).Run(cache_wrapper->CreateHandle(), error);
      },
      std::move(callback));

  // We use our standard wrapping to ensure that we execute our adapter
  // callback on the correct current sequence.
  adapter_callback =
      WrapCallbackForCurrentSequence(std::move(adapter_callback));

  // Passing |cache_wrapper| across sequence boundaries is safe because
  // we are guaranteed this is the only reference to the object.
  inner_.Post(FROM_HERE, &Inner::OpenCache, std::move(cache_wrapper),
              cache_name, trace_id, std::move(adapter_callback));
}

void CrossSequenceCacheStorage::HasCache(const std::string& cache_name,
                                         int64_t trace_id,
                                         BoolAndErrorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_.Post(FROM_HERE, &Inner::HasCache, cache_name, trace_id,
              WrapCallbackForCurrentSequence(std::move(callback)));
}

void CrossSequenceCacheStorage::DoomCache(const std::string& cache_name,
                                          int64_t trace_id,
                                          ErrorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_.Post(FROM_HERE, &Inner::DoomCache, cache_name, trace_id,
              WrapCallbackForCurrentSequence(std::move(callback)));
}

void CrossSequenceCacheStorage::EnumerateCaches(
    int64_t trace_id,
    EnumerateCachesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_.Post(FROM_HERE, &Inner::EnumerateCaches, trace_id,
              WrapCallbackForCurrentSequence(std::move(callback)));
}

void CrossSequenceCacheStorage::MatchCache(
    const std::string& cache_name,
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr match_options,
    CacheStorageSchedulerPriority priority,
    int64_t trace_id,
    CacheStorageCache::ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_.Post(FROM_HERE, &Inner::MatchCache, cache_name, std::move(request),
              std::move(match_options), priority, trace_id,
              WrapCallbackForCurrentSequence(std::move(callback)));
}

void CrossSequenceCacheStorage::MatchAllCaches(
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::CacheQueryOptionsPtr match_options,
    CacheStorageSchedulerPriority priority,
    int64_t trace_id,
    CacheStorageCache::ResponseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_.Post(FROM_HERE, &Inner::MatchAllCaches, std::move(request),
              std::move(match_options), priority, trace_id,
              WrapCallbackForCurrentSequence(std::move(callback)));
}

void CrossSequenceCacheStorage::WriteToCache(
    const std::string& cache_name,
    blink::mojom::FetchAPIRequestPtr request,
    blink::mojom::FetchAPIResponsePtr response,
    int64_t trace_id,
    ErrorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  inner_.Post(FROM_HERE, &Inner::WriteToCache, cache_name, std::move(request),
              std::move(response), trace_id,
              WrapCallbackForCurrentSequence(std::move(callback)));
}

CrossSequenceCacheStorage::~CrossSequenceCacheStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace content
