// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/id_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "content/browser/cache_storage/blob_storage_context_wrapper.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_handle.h"
#include "content/browser/cache_storage/cache_storage_scheduler_types.h"
#include "content/browser/cache_storage/scoped_writable_entry.h"
#include "content/common/content_export.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/disk_cache.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace storage {
class QuotaManagerProxy;
}  // namespace storage

namespace content {
class CacheStorageBlobToDiskCache;
class CacheStorageCacheEntryHandler;
class CacheStorageCacheObserver;
class CacheStorageScheduler;
class CacheStorage;
struct PutContext;

namespace proto {
class CacheMetadata;
}  // namespace proto

namespace cache_storage_cache_unittest {
class TestCacheStorageCache;
class CacheStorageCacheTest;
}  // namespace cache_storage_cache_unittest

// Represents a ServiceWorker Cache as seen in:
//
//  https://w3c.github.io/ServiceWorker/#cache-interface
//
// The asynchronous methods are executed serially. Callbacks to the public
// functions will be called so long as the cache object lives. Client code must
// hold a `CacheStorageCacheHandle` for the duration of operations, otherwise
// the operation may be cancelled in some circumstances.
class CONTENT_EXPORT CacheStorageCache {
 public:
  using CacheEntriesCallback =
      base::OnceCallback<void(blink::mojom::CacheStorageError,
                              std::vector<blink::mojom::CacheEntryPtr>)>;
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

  using SizeCallback = base::OnceCallback<void(int64_t)>;
  using SizePaddingCallback = base::OnceCallback<void(int64_t, int64_t)>;

  // The stream index for a cache Entry. This cannot be extended without changes
  // in the Entry implementation. INDEX_SIDE_DATA is used for storing any
  // additional data, such as response side blobs or request bodies.
  enum EntryIndex {
    INDEX_INVALID = -1,
    INDEX_HEADERS = 0,
    INDEX_RESPONSE_BODY,
    INDEX_SIDE_DATA
  };

  static std::unique_ptr<CacheStorageCache> CreateMemoryCache(
      const storage::BucketLocator& bucket_locator,
      storage::mojom::CacheStorageOwner owner,
      const std::string& cache_name,
      CacheStorage* cache_storage,
      scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      scoped_refptr<BlobStorageContextWrapper> blob_storage_context);
  static std::unique_ptr<CacheStorageCache> CreatePersistentCache(
      const storage::BucketLocator& bucket_locator,
      storage::mojom::CacheStorageOwner owner,
      const std::string& cache_name,
      CacheStorage* cache_storage,
      const base::FilePath& path,
      scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      scoped_refptr<BlobStorageContextWrapper> blob_storage_context,
      int64_t cache_size,
      int64_t cache_padding);
  static int32_t GetResponsePaddingVersion();

  // Returns ERROR_TYPE_NOT_FOUND if not found.
  void Match(blink::mojom::FetchAPIRequestPtr request,
             blink::mojom::CacheQueryOptionsPtr match_options,
             CacheStorageSchedulerPriority priority,
             int64_t trace_id,
             ResponseCallback callback);

  // Returns blink::mojom::CacheStorageError::kSuccess and matched
  // responses in this cache. If there are no responses, returns
  // blink::mojom::CacheStorageError::kSuccess and an empty vector.
  void MatchAll(blink::mojom::FetchAPIRequestPtr request,
                blink::mojom::CacheQueryOptionsPtr match_options,
                int64_t trace_id,
                ResponsesCallback callback);

  // Writes the side data (ex: V8 code cache) for the specified cache entry.
  // If it doesn't exist, or the |expected_response_time| differs from the
  // entry's, blink::mojom::CacheStorageError::kErrorNotFound is returned.
  // Note: This "side data" is same meaning as "metadata" in HTTPCache. We use
  // "metadata" in cache_storage.proto for the pair of headers of a request and
  // a response. To avoid the confusion we use "side data" here.
  void WriteSideData(ErrorCallback callback,
                     const GURL& url,
                     base::Time expected_response_time,
                     int64_t trace_id,
                     scoped_refptr<net::IOBuffer> buffer,
                     int buf_len);

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
  void BatchOperation(std::vector<blink::mojom::BatchOperationPtr> operations,
                      int64_t trace_id,
                      VerboseErrorCallback callback,
                      BadMessageCallback bad_message_callback);
  void BatchDidGetBucketSpaceRemaining(
      std::vector<blink::mojom::BatchOperationPtr> operations,
      int64_t trace_id,
      VerboseErrorCallback callback,
      BadMessageCallback bad_message_callback,
      std::optional<std::string> message,
      uint64_t space_required,
      uint64_t side_data_size,
      storage::QuotaErrorOr<int64_t> space_remaining);

  // Returns blink::mojom::CacheStorageError::kSuccess and a vector of
  // requests if there are no errors.
  void Keys(blink::mojom::FetchAPIRequestPtr request,
            blink::mojom::CacheQueryOptionsPtr options,
            int64_t trace_id,
            RequestsCallback callback);

  // Closes the backend. Future operations that require the backend
  // will exit early. Close should only be called once per CacheStorageCache.
  void Close(base::OnceClosure callback);

  // The size of the cache's contents.  The callback reports the padded
  // size.  If you want the unpadded size you may call the cache_size()
  // getter method on the cache object when the callback is invoked; the
  // getter will have an up-to-date value at that point.
  void Size(SizeCallback callback);

  // Gets the cache's size, closes the backend, and then runs |callback| with
  // the cache's size.  As per the comment for Size(), this also returns the
  // padded size.
  void GetSizeThenClose(SizeCallback callback);

  // Puts the request/response pair in the cache. This is a public member to
  // directly bypass the batch operations and write into the cache. This is used
  // by non-CacheAPI owners. The Cache Storage API uses batch operations defined
  // in the dispatcher.
  void Put(blink::mojom::FetchAPIRequestPtr request,
           blink::mojom::FetchAPIResponsePtr response,
           int64_t trace_id,
           ErrorCallback callback);

  // Similar to MatchAll, but returns the associated requests as well.
  void GetAllMatchedEntries(blink::mojom::FetchAPIRequestPtr request,
                            blink::mojom::CacheQueryOptionsPtr match_options,
                            int64_t trace_id,
                            CacheEntriesCallback callback);

  // Try to determine the initialization state of the cache.  Unknown may be
  // returned for cross-sequence clients using the cross-sequence wrappers.
  enum class InitState { Unknown, Initializing, Initialized };
  InitState GetInitState() const;

  CacheStorageCache(const CacheStorageCache&) = delete;
  CacheStorageCache& operator=(const CacheStorageCache&) = delete;

  // Async operations in progress will cancel and not run their callbacks.
  virtual ~CacheStorageCache();

  base::FilePath path() const { return path_; }

  std::string cache_name() const { return cache_name_; }

  int64_t cache_size() const { return cache_size_; }

  int64_t cache_padding() const { return cache_padding_; }

  // Return the total cache size (actual size + padding). If either is unknown
  // then CacheStorage::kSizeUnknown is returned.
  int64_t PaddedCacheSize() const;

  // Set the one observer that will be notified of changes to this cache.
  // Note: Either the observer must have a lifetime longer than this instance
  // or call SetObserver(nullptr) to stop receiving notification of changes.
  void SetObserver(CacheStorageCacheObserver* observer);

  static size_t EstimatedStructSize(
      const blink::mojom::FetchAPIRequestPtr& request);

  base::WeakPtr<CacheStorageCache> AsWeakPtr();

  // Create a handle that will hold the CacheStorageCache alive. Client code
  // should hold one of these handles while waiting for operation callbacks to
  // be invoked.
  //
  // Note, its still possible for the CacheStorageCache to be deleted even if
  // there are outstanding handle references. This can occur when the user
  // triggers a storage wipe, for example. The handle value should be treated
  // as a weak pointer.
  CacheStorageCacheHandle CreateHandle();
  void AddHandleRef();
  void DropHandleRef();
  bool IsUnreferenced() const;

  //  the default scheduler with a customized scheduler for testing.
  // The current scheduler must be idle.
  void SetSchedulerForTesting(std::unique_ptr<CacheStorageScheduler> scheduler);

  static CacheStorageCache* From(const CacheStorageCacheHandle& handle) {
    return static_cast<CacheStorageCache*>(handle.value());
  }

 private:
  // QueryCache types:
  enum QueryCacheFlags {
    QUERY_CACHE_REQUESTS = 0x1,
    QUERY_CACHE_RESPONSES_WITH_BODIES = 0x2,
    QUERY_CACHE_RESPONSES_NO_BODIES = 0x4,
    QUERY_CACHE_ENTRIES = 0x8,
  };

  // The backend progresses from uninitialized, to open, to closed, and cannot
  // reverse direction.  The open step may be skipped.
  enum BackendState {
    BACKEND_UNINITIALIZED,  // No backend, create backend on first operation.
    BACKEND_OPEN,           // Backend can be used.
    BACKEND_CLOSED          // Backend cannot be used.  All ops should fail.
  };

  friend class base::RefCounted<CacheStorageCache>;
  friend class cache_storage_cache_unittest::TestCacheStorageCache;
  friend class cache_storage_cache_unittest::CacheStorageCacheTest;

  struct QueryCacheContext;
  struct QueryCacheResult;
  struct BatchInfo;

  using QueryTypes = int32_t;
  using QueryCacheResults = std::vector<QueryCacheResult>;
  using QueryCacheCallback =
      base::OnceCallback<void(blink::mojom::CacheStorageError,
                              std::unique_ptr<QueryCacheResults>)>;
  using Entries = std::vector<disk_cache::Entry*>;
  using BlobToDiskCacheIDMap =
      base::IDMap<std::unique_ptr<CacheStorageBlobToDiskCache>>;

  CacheStorageCache(
      const storage::BucketLocator& bucket_locator,
      storage::mojom::CacheStorageOwner owner,
      const std::string& cache_name,
      const base::FilePath& path,
      CacheStorage* cache_storage,
      scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner,
      scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
      scoped_refptr<BlobStorageContextWrapper> blob_storage_context,
      int64_t cache_size,
      int64_t cache_padding);

  // Callback passed to operations. If |error| is a real error, invokes
  // |error_callback|. Always invokes |completion_closure| to signal
  // completion.
  void BatchDidOneOperation(BatchInfo& batch_status,
                            blink::mojom::CacheStorageError error);

  // Runs |callback| with matching requests/response data. The data provided
  // in the QueryCacheResults depends on the |query_type|. If |query_type| is
  // CACHE_ENTRIES then only out_entries is valid. If |query_type| is REQUESTS
  // then only out_requests is valid. If |query_type| is
  // REQUESTS_AND_RESPONSES then only out_requests, out_responses, and
  // out_blob_data_handles are valid.
  void QueryCache(blink::mojom::FetchAPIRequestPtr request,
                  blink::mojom::CacheQueryOptionsPtr options,
                  QueryTypes query_types,
                  CacheStorageSchedulerPriority priority,
                  QueryCacheCallback callback);
  void QueryCacheDidOpenFastPath(
      std::unique_ptr<QueryCacheContext> query_cache_context,
      disk_cache::EntryResult result);
  void QueryCacheOpenNextEntry(
      std::unique_ptr<QueryCacheContext> query_cache_context);
  void QueryCacheFilterEntry(
      std::unique_ptr<QueryCacheContext> query_cache_context,
      disk_cache::EntryResult result);
  void QueryCacheDidReadMetadata(
      std::unique_ptr<QueryCacheContext> query_cache_context,
      disk_cache::ScopedEntryPtr entry,
      std::unique_ptr<proto::CacheMetadata> metadata);
  void QueryCacheUpgradePadding(
      std::unique_ptr<QueryCacheContext> query_cache_context,
      disk_cache::ScopedEntryPtr entry,
      std::unique_ptr<proto::CacheMetadata> metadata);
  static bool QueryCacheResultCompare(const QueryCacheResult& lhs,
                                      const QueryCacheResult& rhs);
  static size_t EstimatedResponseSizeWithoutBlob(
      const blink::mojom::FetchAPIResponse& response);

  // Match callbacks
  void MatchImpl(blink::mojom::FetchAPIRequestPtr request,
                 blink::mojom::CacheQueryOptionsPtr match_options,
                 int64_t trace_id,
                 CacheStorageSchedulerPriority priority,
                 ResponseCallback callback);
  void MatchDidMatchAll(
      ResponseCallback callback,
      blink::mojom::CacheStorageError match_all_error,
      std::vector<blink::mojom::FetchAPIResponsePtr> match_all_responses);

  // MatchAll callbacks
  void MatchAllImpl(blink::mojom::FetchAPIRequestPtr request,
                    blink::mojom::CacheQueryOptionsPtr options,
                    int64_t trace_id,
                    CacheStorageSchedulerPriority priority,
                    ResponsesCallback callback);
  void MatchAllDidQueryCache(
      ResponsesCallback callback,
      int64_t trace_id,
      blink::mojom::CacheStorageError error,
      std::unique_ptr<QueryCacheResults> query_cache_results);

  // Utility method to write metadata headers to an entry.
  using WriteMetadataCallback =
      base::OnceCallback<void(int exepected_bytes, int rv)>;
  void WriteMetadata(disk_cache::Entry* entry,
                     const proto::CacheMetadata& metadata,
                     WriteMetadataCallback callback);

  // WriteSideData callbacks
  void WriteSideDataDidGetBucketSpaceRemaining(
      ErrorCallback callback,
      const GURL& url,
      base::Time expected_response_time,
      int64_t trace_id,
      scoped_refptr<net::IOBuffer> buffer,
      int buf_len,
      storage::QuotaErrorOr<int64_t> space_remaining);

  void WriteSideDataImpl(ErrorCallback callback,
                         const GURL& url,
                         base::Time expected_response_time,
                         int64_t trace_id,
                         scoped_refptr<net::IOBuffer> buffer,
                         int buf_len);
  void WriteSideDataDidGetUsageAndQuota(
      ErrorCallback callback,
      const GURL& url,
      base::Time expected_response_time,
      int64_t trace_id,
      scoped_refptr<net::IOBuffer> buffer,
      int buf_len,
      blink::mojom::QuotaStatusCode status_code,
      int64_t usage,
      int64_t quota);
  void WriteSideDataDidOpenEntry(ErrorCallback callback,
                                 base::Time expected_response_time,
                                 int64_t trace_id,
                                 scoped_refptr<net::IOBuffer> buffer,
                                 int buf_len,
                                 disk_cache::EntryResult result);
  void WriteSideDataDidReadMetaData(
      ErrorCallback callback,
      base::Time expected_response_time,
      int64_t trace_id,
      scoped_refptr<net::IOBuffer> buffer,
      int buf_len,
      ScopedWritableEntry entry,
      std::unique_ptr<proto::CacheMetadata> headers);
  void WriteSideDataDidWrite(
      ErrorCallback callback,
      ScopedWritableEntry entry,
      int expected_bytes,
      std::unique_ptr<content::proto::CacheMetadata> metadata,
      int64_t trace_id,
      int rv);
  void WriteSideDataDidWriteMetadata(ErrorCallback callback,
                                     ScopedWritableEntry entry,
                                     int64_t padding,
                                     int64_t side_data_padding,
                                     int expected_bytes,
                                     int rv);
  void WriteSideDataComplete(ErrorCallback callback,
                             ScopedWritableEntry entry,
                             int64_t padding,
                             int64_t side_data_padding,
                             blink::mojom::CacheStorageError error);

  // Puts the request and response object in the cache. The response body (if
  // present) is stored in the cache, but not the request body. Returns OK on
  // success.
  void Put(blink::mojom::BatchOperationPtr operation,
           int64_t trace_id,
           ErrorCallback callback);
  void PutImpl(std::unique_ptr<PutContext> put_context);
  void PutDidDeleteEntry(std::unique_ptr<PutContext> put_context,
                         blink::mojom::CacheStorageError error);
  void PutDidGetUsageAndQuota(std::unique_ptr<PutContext> put_context,
                              blink::mojom::QuotaStatusCode status_code,
                              int64_t usage,
                              int64_t quota);
  void PutDidCreateEntry(std::unique_ptr<PutContext> put_context,
                         disk_cache::EntryResult result);
  void PutDidWriteHeaders(std::unique_ptr<PutContext> put_context,
                          int64_t padding,
                          int64_t side_data_padding,
                          int expected_bytes,
                          int rv);
  void PutWriteBlobToCache(std::unique_ptr<PutContext> put_context,
                           int disk_cache_body_index);
  void PutDidWriteBlobToCache(std::unique_ptr<PutContext> put_context,
                              BlobToDiskCacheIDMap::KeyType blob_to_cache_key,
                              int disk_cache_body_index,
                              ScopedWritableEntry entry,
                              bool success);
  void PutWriteBlobToCacheComplete(std::unique_ptr<PutContext> put_context,
                                   int disk_cache_body_index,
                                   ScopedWritableEntry entry,
                                   int rv);
  void PutComplete(std::unique_ptr<PutContext> put_context,
                   blink::mojom::CacheStorageError error);

  // Asynchronously calculates the current cache size, notifies the quota
  // manager of any change from the last report, and sets cache_size_ to the new
  // size.
  void UpdateCacheSize(base::OnceClosure callback);
  void UpdateCacheSizeGotSize(CacheStorageCacheHandle,
                              base::OnceClosure callback,
                              int64_t current_cache_size);
  void UpdateCacheSizeNotifiedStorageModified(base::OnceClosure callback);

  // GetAllMatchedEntries callbacks.
  void GetAllMatchedEntriesImpl(blink::mojom::FetchAPIRequestPtr request,
                                blink::mojom::CacheQueryOptionsPtr options,
                                int64_t trace_id,
                                CacheEntriesCallback callback);
  void GetAllMatchedEntriesDidQueryCache(
      int64_t trace_id,
      CacheEntriesCallback callback,
      blink::mojom::CacheStorageError error,
      std::unique_ptr<QueryCacheResults> query_cache_results);

  // Returns ERROR_NOT_FOUND if not found. Otherwise deletes and returns OK.
  void Delete(blink::mojom::BatchOperationPtr operation,
              ErrorCallback callback);
  void DeleteImpl(blink::mojom::FetchAPIRequestPtr request,
                  blink::mojom::CacheQueryOptionsPtr match_options,
                  ErrorCallback callback);
  void DeleteDidQueryCache(
      ErrorCallback callback,
      blink::mojom::CacheStorageError error,
      std::unique_ptr<QueryCacheResults> query_cache_results);

  // Keys callbacks.
  void KeysImpl(blink::mojom::FetchAPIRequestPtr request,
                blink::mojom::CacheQueryOptionsPtr options,
                int64_t trace_id,
                RequestsCallback callback);
  void KeysDidQueryCache(
      RequestsCallback callback,
      int64_t trace_id,
      blink::mojom::CacheStorageError error,
      std::unique_ptr<QueryCacheResults> query_cache_results);

  void CloseImpl(base::OnceClosure callback);

  void SizeImpl(SizeCallback callback);

  void GetSizeThenCloseDidGetSize(SizeCallback callback, int64_t cache_size);

  // Loads the backend and calls the callback with the result (true for
  // success). The callback will always be called. Virtual for tests.
  virtual void CreateBackend(ErrorCallback callback);
  void CreateBackendDidCreate(ErrorCallback callback,
                              disk_cache::BackendResult result);

  // Calculate the size and padding of the cache.
  void CalculateCacheSizePadding(SizePaddingCallback callback);
  void CalculateCacheSizePaddingGotSize(SizePaddingCallback callback,
                                        int64_t cache_size);
  void PaddingDidQueryCache(
      SizePaddingCallback callback,
      int64_t cache_size,
      blink::mojom::CacheStorageError error,
      std::unique_ptr<QueryCacheResults> query_cache_results);

  // Calculate the size (but not padding) of the cache.
  void CalculateCacheSize(net::Int64CompletionOnceCallback callback);

  void InitBackend();
  void InitDidCreateBackend(base::OnceClosure callback,
                            blink::mojom::CacheStorageError cache_create_error);
  void InitGotCacheSize(base::OnceClosure callback,
                        blink::mojom::CacheStorageError cache_create_error,
                        int64_t cache_size);
  void InitGotCacheSizeAndPadding(
      base::OnceClosure callback,
      blink::mojom::CacheStorageError cache_create_error,
      int64_t cache_size,
      int64_t cache_padding);
  void DeleteBackendCompletedIO();

  // Calculate the required safe space to put the entry in the cache.
  base::CheckedNumeric<uint64_t> CalculateRequiredSafeSpaceForPut(
      const blink::mojom::BatchOperationPtr& operation);
  base::CheckedNumeric<uint64_t> CalculateRequiredSafeSpaceForRequest(
      const blink::mojom::FetchAPIRequestPtr& request);
  base::CheckedNumeric<uint64_t> CalculateRequiredSafeSpaceForResponse(
      const blink::mojom::FetchAPIResponsePtr& response);

  // Wrap |callback| in order to reference a CacheStorageCacheHandle
  // for the duration of an asynchronous operation.  We must keep this
  // self reference for a couple reasons.  First, we must allow any writes
  // to cleanly complete in order to avoid truncated entries.  In addition,
  // we must keep the cache and its disk_cache backend alive until all
  // open Entry objects are destroyed to avoid having a second backend
  // opened by another CacheStorageCache clobbering the entries.
  template <typename... Args>
  base::OnceCallback<void(Args...)> WrapCallbackWithHandle(
      base::OnceCallback<void(Args...)> callback) {
    return base::BindOnce(&CacheStorageCache::RunWithHandle<Args...>,
                          weak_ptr_factory_.GetWeakPtr(), CreateHandle(),
                          std::move(callback));
  }

  // Invoked by wrapped callbacks with the CacheStorageCacheHandle passed
  // as a parameter.  The handle is kept alive here simply to maintain
  // a self-reference during the operation.
  template <typename... Args>
  void RunWithHandle(CacheStorageCacheHandle handle,
                     base::OnceCallback<void(Args...)> callback,
                     Args... args) {
    std::move(callback).Run(std::forward<Args>(args)...);
    // |handle| is destroyed after running the inner wrapped callback.
  }

  // Be sure to check |backend_state_| before use.
  std::unique_ptr<disk_cache::Backend> backend_;

  const storage::BucketLocator bucket_locator_;
  const storage::mojom::CacheStorageOwner owner_;
  const std::string cache_name_;
  const base::FilePath path_;

  // Raw pointer is safe because the CacheStorage instance owns this
  // CacheStorageCache object.
  const raw_ptr<CacheStorage> cache_storage_;

  // A handle that is used to keep the owning CacheStorage instance referenced
  // as long this cache object is also referenced.
  CacheStorageHandle cache_storage_handle_;

  const scoped_refptr<base::SequencedTaskRunner> scheduler_task_runner_;
  const scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
  BackendState backend_state_ = BACKEND_UNINITIALIZED;
  std::unique_ptr<CacheStorageScheduler> scheduler_;
  bool initializing_ = false;
  // The actual cache size (not including padding).
  int64_t cache_size_;
  int64_t cache_padding_ = 0;
  int64_t last_reported_size_ = 0;
  size_t max_query_size_bytes_;
  size_t handle_ref_count_ = 0;
  int query_cache_recursive_depth_ = 0;
  raw_ptr<CacheStorageCacheObserver> cache_observer_;
  std::unique_ptr<CacheStorageCacheEntryHandler> cache_entry_handler_;

  // Owns the elements of the list
  BlobToDiskCacheIDMap active_blob_to_disk_cache_writers_;

  // Whether or not to store data in disk or memory.
  const bool memory_only_;

  // Active while waiting for the backend to finish its closing up, and contains
  // the callback passed to CloseImpl.
  base::OnceClosure post_backend_closed_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CacheStorageCache> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_CACHE_H_
