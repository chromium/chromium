// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_scheduler_types.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "url/origin.h"

namespace content {

// Represents a ServiceWorker Cache as seen in:
//
//  https://w3c.github.io/ServiceWorker/#cache-interface
//
// The asynchronous methods are executed serially. Callbacks to the public
// functions will be called so long as the cache object lives. It is important
// to for client code hold a |CacheStorageCacheHandle| to the cache for the
// duration of any operations. Otherwise it is possible the operation may
// get cancelled in some circumstances.
class CONTENT_EXPORT CacheStorageCache {
 public:
  using CacheEntry = std::pair<blink::mojom::FetchAPIRequestPtr,
                               blink::mojom::FetchAPIResponsePtr>;
  using CacheEntriesCallback =
      base::OnceCallback<void(blink::mojom::CacheStorageError,
                              std::vector<CacheEntry>)>;
  using ErrorCallback =
      base::OnceCallback<void(blink::mojom::CacheStorageError)>;
  using VerboseErrorCallback =
      base::OnceCallback<void(blink::mojom::CacheStorageVerboseErrorPtr)>;
  using BadMessageCallback = base::OnceCallback<void()>;
  using ResponseCallback =
      base::OnceCallback<void(blink::mojom::CacheStorageError,
                              blink::mojom::FetchAPIResponsePtr)>;
  using ResponsesCallback =
      base::OnceCallback<void(blink::mojom::CacheStorageError,
                              std::vector<blink::mojom::FetchAPIResponsePtr>)>;
  using Requests = std::vector<blink::mojom::FetchAPIRequestPtr>;
  using RequestsCallback =
      base::OnceCallback<void(blink::mojom::CacheStorageError,
                              std::unique_ptr<Requests>)>;

  // The stream index for a cache Entry. This cannot be extended without changes
  // in the Entry implementation. INDEX_SIDE_DATA is used for storing any
  // additional data, such as response side blobs or request bodies.
  enum EntryIndex {
    INDEX_INVALID = -1,
    INDEX_HEADERS = 0,
    INDEX_RESPONSE_BODY,
    INDEX_SIDE_DATA
  };

  // Create a handle that will hold the CacheStorageCache alive. Client code
  // should hold one of these handles while waiting for operation callbacks to
  // be invoked.
  //
  // Note, its still possible for the CacheStorageCache to be deleted even if
  // there are outstanding handle references. This can occur when the user
  // triggers a storage wipe, for example. The handle value should be treated
  // as a weak pointer.
  virtual CacheStorageCacheHandle CreateHandle() = 0;
  virtual void AddHandleRef() = 0;
  virtual void DropHandleRef() = 0;
  virtual bool IsUnreferenced() const = 0;

  // Returns ERROR_TYPE_NOT_FOUND if not found.
  virtual void Match(blink::mojom::FetchAPIRequestPtr request,
                     blink::mojom::CacheQueryOptionsPtr match_options,
                     CacheStorageSchedulerPriority priority,
                     int64_t trace_id,
                     ResponseCallback callback) = 0;

  // Returns blink::mojom::CacheStorageError::kSuccess and matched
  // responses in this cache. If there are no responses, returns
  // blink::mojom::CacheStorageError::kSuccess and an empty vector.
  virtual void MatchAll(blink::mojom::FetchAPIRequestPtr request,
                        blink::mojom::CacheQueryOptionsPtr match_options,
                        int64_t trace_id,
                        ResponsesCallback callback) = 0;

  // Writes the side data (ex: V8 code cache) for the specified cache entry.
  // If it doesn't exist, or the |expected_response_time| differs from the
  // entry's, blink::mojom::CacheStorageError::kErrorNotFound is returned.
  // Note: This "side data" is same meaning as "metadata" in HTTPCache. We use
  // "metadata" in cache_storage.proto for the pair of headers of a request and
  // a response. To avoid the confusion we use "side data" here.
  virtual void WriteSideData(ErrorCallback callback,
                             const GURL& url,
                             base::Time expected_response_time,
                             int64_t trace_id,
                             scoped_refptr<net::IOBuffer> buffer,
                             int buf_len) = 0;

  // Runs given batch operations. This corresponds to the Batch Cache Operations
  // algorithm in the spec.
  //
  // |operations| cannot mix PUT and DELETE operations and cannot contain
  // multiple DELETE operations.
  //
  // In the case of the PUT operation, puts request and response objects in the
  // cache and returns OK when all operations are successfully completed.
  // In the case of the DELETE operation, returns ERROR_NOT_FOUND if a specified
  // entry is not found. Otherwise deletes it and returns OK.
  //
  // TODO(nhiroki): This function should run all operations atomically.
  // http://crbug.com/486637
  virtual void BatchOperation(
      std::vector<blink::mojom::BatchOperationPtr> operations,
      int64_t trace_id,
      VerboseErrorCallback callback,
      BadMessageCallback bad_message_callback) = 0;

  // Returns blink::mojom::CacheStorageError::kSuccess and a vector of
  // requests if there are no errors.
  virtual void Keys(blink::mojom::FetchAPIRequestPtr request,
                    blink::mojom::CacheQueryOptionsPtr options,
                    int64_t trace_id,
                    RequestsCallback callback) = 0;

  // Puts the request/response pair in the cache. This is a public member to
  // directly bypass the batch operations and write into the cache. This is used
  // by non-CacheAPI owners. The Cache Storage API uses batch operations defined
  // in the dispatcher.
  virtual void Put(blink::mojom::FetchAPIRequestPtr request,
                   blink::mojom::FetchAPIResponsePtr response,
                   int64_t trace_id,
                   ErrorCallback callback) = 0;

  // Similar to MatchAll, but returns the associated requests as well.
  virtual void GetAllMatchedEntries(
      blink::mojom::FetchAPIRequestPtr request,
      blink::mojom::CacheQueryOptionsPtr match_options,
      int64_t trace_id,
      CacheEntriesCallback callback) = 0;

  // Try to determine the initialization state of the cache.  Unknown may be
  // returned for cross-sequence clients using the cross-sequence wrappers.
  enum class InitState { Unknown, Initializing, Initialized };
  virtual InitState GetInitState() const = 0;

 protected:
  virtual ~CacheStorageCache() = default;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_H_
