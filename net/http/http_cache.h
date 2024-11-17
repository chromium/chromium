// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares a HttpTransactionFactory implementation that can be
// layered on top of another HttpTransactionFactory to add HTTP caching.  The
// caching logic follows RFC 7234 (any exceptions are called out in the code).
//
// The HttpCache takes a disk_cache::Backend as a parameter, and uses that for
// the cache storage.
//
// See HttpTransactionFactory and HttpTransaction for more details.

#ifndef NET_HTTP_HTTP_CACHE_H_
#define NET_HTTP_HTTP_CACHE_H_

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "base/containers/lru_cache.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/hash/sha1.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/clock.h"
#include "build/build_config.h"
#include "net/base/cache_type.h"
#include "net/base/completion_once_callback.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_transaction_factory.h"

class GURL;

namespace url {
class Origin;
}

namespace net {

class HttpNetworkSession;
class HttpResponseInfo;
class NetLog;
class NetworkIsolationKey;
struct HttpRequestInfo;

class NET_EXPORT HttpCache : public HttpTransactionFactory {
 public:
  // The cache mode of operation.
  enum Mode {
    // Normal mode just behaves like a standard web cache.
    NORMAL = 0,
    // Disables reads and writes from the cache.
    // Equivalent to setting LOAD_DISABLE_CACHE on every request.
    DISABLE
  };

  // A BackendFactory creates a backend object to be used by the HttpCache.
  class NET_EXPORT BackendFactory {
   public:
    virtual ~BackendFactory() = default;

    // The actual method to build the backend. The return value and `callback`
    // conventions match disk_cache::CreateCacheBackend
    //
    // The implementation must not access the factory object after invoking the
    // `callback` because the object can be deleted from within the callback.
    virtual disk_cache::BackendResult CreateBackend(
        NetLog* net_log,
        base::OnceCallback<void(disk_cache::BackendResult)> callback) = 0;

#if BUILDFLAG(IS_ANDROID)
    virtual void SetAppStatusListenerGetter(
        disk_cache::ApplicationStatusListenerGetter
            app_status_listener_getter) {}
#endif
  };

  // A default backend factory for the common use cases.
  class NET_EXPORT DefaultBackend : public BackendFactory {
   public:
    // `file_operations_factory` can be null, in that case
    // TrivialFileOperationsFactory is used. `path` is the destination for any
    // files used by the backend. If `max_bytes` is  zero, a default value
    // will be calculated automatically.
    DefaultBackend(CacheType type,
                   BackendType backend_type,
                   scoped_refptr<disk_cache::BackendFileOperationsFactory>
                       file_operations_factory,
                   const base::FilePath& path,
                   int max_bytes,
                   bool hard_reset);
    ~DefaultBackend() override;

    // Returns a factory for an in-memory cache.
    static std::unique_ptr<BackendFactory> InMemory(int max_bytes);

    // BackendFactory implementation.
    disk_cache::BackendResult CreateBackend(
        NetLog* net_log,
        base::OnceCallback<void(disk_cache::BackendResult)> callback) override;

#if BUILDFLAG(IS_ANDROID)
    void SetAppStatusListenerGetter(disk_cache::ApplicationStatusListenerGetter
                                        app_status_listener_getter) override;
#endif

   private:
    CacheType type_;
    BackendType backend_type_;
    const scoped_refptr<disk_cache::BackendFileOperationsFactory>
        file_operations_factory_;
    const base::FilePath path_;
    int max_bytes_;
    bool hard_reset_;
#if BUILDFLAG(IS_ANDROID)
    disk_cache::ApplicationStatusListenerGetter app_status_listener_getter_;
#endif
  };

  // Whether a transaction can join parallel writing or not is a function of the
  // transaction as well as the current writers (if present). This enum
  // captures that decision as well as when a Writers object is first created.
  // This is also used to log metrics so should be consistent with the values in
  // enums.xml and should only be appended to.
  enum ParallelWritingPattern {
    // Used as the default value till the transaction is in initial headers
    // phase.
    PARALLEL_WRITING_NONE,
    // The transaction creates a writers object. This is only logged for
    // transactions that did not fail to join existing writers earlier.
    PARALLEL_WRITING_CREATE,
    // The transaction joins existing writers.
    PARALLEL_WRITING_JOIN,
    // The transaction cannot join existing writers since either itself or
    // existing writers instance is serving a range request.
    PARALLEL_WRITING_NOT_JOIN_RANGE,
    // The transaction cannot join existing writers since either itself or
    // existing writers instance is serving a non GET request.
    PARALLEL_WRITING_NOT_JOIN_METHOD_NOT_GET,
    // The transaction cannot join existing writers since it does not have cache
    // write privileges.
    PARALLEL_WRITING_NOT_JOIN_READ_ONLY,
    // Writers does not exist and the transaction does not need to create one
    // since it is going to read from the cache.
    PARALLEL_WRITING_NONE_CACHE_READ,
    // Unable to join since the entry is too big for cache backend to handle.
    PARALLEL_WRITING_NOT_JOIN_TOO_BIG_FOR_CACHE,
    // On adding a value here, make sure to add in enums.xml as well.
    PARALLEL_WRITING_MAX
  };

  // The number of minutes after a resource is prefetched that it can be used
  // again without validation.
  static const int kPrefetchReuseMins = 5;

  // Initialize the cache from its component parts. |network_layer| and
  // |backend_factory| will be destroyed when the HttpCache is.
  HttpCache(std::unique_ptr<HttpTransactionFactory> network_layer,
            std::unique_ptr<BackendFactory> backend_factory);

  HttpCache(const HttpCache&) = delete;
  HttpCache& operator=(const HttpCache&) = delete;

  ~HttpCache() override;

  HttpTransactionFactory* network_layer() { return network_layer_.get(); }

  using GetBackendResult = std::pair<int, raw_ptr<disk_cache::Backend>>;
  using GetBackendCallback = base::OnceCallback<void(GetBackendResult)>;
  // Retrieves the cache backend for this HttpCache instance. If the backend
  // is not initialized yet, this method will initialize it. The integer portion
  // of the return value is a network error code, and it could be
  // ERR_IO_PENDING, in which case the `callback` will be notified when the
  // operation completes.
  // `callback` will get cancelled if the HttpCache is destroyed.
  GetBackendResult GetBackend(GetBackendCallback callback);

  // Returns the current backend (can be NULL).
  disk_cache::Backend* GetCurrentBackend() const;

  // Given a header data blob, convert it to a response info object.
  static bool ParseResponseInfo(base::span<const uint8_t> data,
                                HttpResponseInfo* response_info,
                                bool* response_truncated);

  // Get/Set the cache's mode.
  void set_mode(Mode value) { mode_ = value; }
  Mode mode() { return mode_; }

  // Get/Set the cache's clock. These are public only for testing.
  void SetClockForTesting(base::Clock* clock) { clock_ = clock; }
  base::Clock* clock() const { return clock_; }

  // Close currently active sockets so that fresh page loads will not use any
  // recycled connections.  For sockets currently in use, they may not close
  // immediately, but they will not be reusable. This is for debugging.
  void CloseAllConnections(int net_error, const char* net_log_reason_utf8);

  // Close all idle connections. Will close all sockets not in active use.
  void CloseIdleConnections(const char* net_log_reason_utf8);

  // Called whenever an external cache in the system reuses the resource
  // referred to by `url`, `http_method`, `network_isolation_key`, and
  // `include_credentials`.
  void OnExternalCacheHit(const GURL& url,
                          const std::string& http_method,
                          const NetworkIsolationKey& network_isolation_key,
                          bool include_credentials);

  // Causes all transactions created after this point to simulate lock timeout
  // and effectively bypass the cache lock whenever there is lock contention.
  void SimulateCacheLockTimeoutForTesting() { bypass_lock_for_test_ = true; }

  // Causes all transactions created after this point to simulate lock timeout
  // and effectively bypass the cache lock whenever there is lock contention
  // after the transaction has completed its headers phase.
  void SimulateCacheLockTimeoutAfterHeadersForTesting() {
    bypass_lock_after_headers_for_test_ = true;
  }

  void DelayAddTransactionToEntryForTesting() {
    delay_add_transaction_to_entry_for_test_ = true;
  }

  // Causes all transactions created after this point to generate a failure
  // when attempting to conditionalize a network request.
  void FailConditionalizationForTest() {
    fail_conditionalization_for_test_ = true;
  }

  // HttpTransactionFactory implementation:
  int CreateTransaction(RequestPriority priority,
                        std::unique_ptr<HttpTransaction>* transaction) override;
  HttpCache* GetCache() override;
  HttpNetworkSession* GetSession() override;

  base::WeakPtr<HttpCache> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

  // Resets the network layer to allow for tests that probe
  // network changes (e.g. host unreachable).  The old network layer is
  // returned to allow for filter patterns that only intercept
  // some creation requests.  Note ownership exchange.
  std::unique_ptr<HttpTransactionFactory>
  SetHttpNetworkTransactionFactoryForTesting(
      std::unique_ptr<HttpTransactionFactory> new_network_layer);

  // Get the URL from the entry's cache key.
  static std::string GetResourceURLFromHttpCacheKey(const std::string& key);

  // Generates the cache key for a request.
  static std::optional<std::string> GenerateCacheKeyForRequest(
      const HttpRequestInfo* request);

  enum class ExperimentMode {
    // No additional partitioning is done for top-level navigations.
    kStandard,
    // A boolean is incorporated into the cache key that is true for
    // renderer-initiated main frame navigations when the request initiator site
    // is cross-site to the URL being navigated to.
    kCrossSiteInitiatorBoolean,
    // The request initiator site is incorporated into the cache key for
    // renderer-initiated main frame navigations when the request initiator site
    // is cross-site to the URL being navigated to. If the request initiator
    // site is opaque, then no caching is performed of the navigated-to
    // document.
    kMainFrameNavigationInitiator,
    // The request initiator site is incorporated into the cache key for all
    // renderer-initiated navigations (including subframe navigations) when the
    // request initiator site is cross-site to the URL being navigated to. If
    // the request initiator site is opaque, then no caching is performed of the
    // navigated-to document. When this scheme is used, the
    // `is-subframe-document-resource` boolean is not incorporated into the
    // cache key, since incorporating the initiator site for subframe
    // navigations
    // should be sufficient for mitigating the attacks that the
    // `is-subframe-document-resource` mitigates.
    kNavigationInitiator,
  };

  // Returns the HTTP Cache partitioning experiment mode currently in use. Only
  // one experiment mode feature flag should be enabled at a time, but if
  // multiple are enabled then `ExperimentMode::kStandard` will be returned.
  static ExperimentMode GetExperimentMode();

  // Enable split cache feature if not already overridden in the feature list.
  // Should only be invoked during process initialization before the HTTP
  // cache is initialized.
  static void SplitCacheFeatureEnableByDefault();

  // Returns true if split cache is enabled either by default or by other means
  // like command line or field trials.
  static bool IsSplitCacheEnabled();

  // Resets g_init_cache and g_enable_split_cache for tests.
  static void ClearGlobalsForTesting();

 private:
  // Types --------------------------------------------------------------------

  // The type of operation represented by a work item.
  enum WorkItemOperation {
    WI_CREATE_BACKEND,
    WI_OPEN_OR_CREATE_ENTRY,
    WI_OPEN_ENTRY,
    WI_CREATE_ENTRY,
    WI_DOOM_ENTRY
  };

  // Disk cache entry data indices.
  enum {
    kResponseInfoIndex = 0,
    kResponseContentIndex,
    kDeprecatedMetadataIndex,
    // Must remain at the end of the enum.
    kNumCacheEntryDataIndices
  };

  class QuicServerInfoFactoryAdaptor;
  class Transaction;
  class WorkItem;
  class Writers;

  friend class WritersTest;
  friend class TestHttpCacheTransaction;
  friend class TestHttpCache;
  friend class Transaction;
  struct PendingOp;  // Info for an entry under construction.

  // To help with testing.
  friend class MockHttpCache;
  friend class HttpCacheIOCallbackTest;

  FRIEND_TEST_ALL_PREFIXES(HttpCacheTest_SplitCacheFeatureEnabled,
                           SplitCacheWithNetworkIsolationKey);
  FRIEND_TEST_ALL_PREFIXES(HttpCacheTest, NonSplitCache);
  FRIEND_TEST_ALL_PREFIXES(HttpCacheTest_SplitCacheFeatureEnabled, SplitCache);
  FRIEND_TEST_ALL_PREFIXES(HttpCacheTest_SplitCacheFeatureEnabled,
                           SplitCacheUsesRegistrableDomain);

  using TransactionList = std::list<raw_ptr<Transaction, CtnExperimental>>;
  using TransactionSet =
      std::unordered_set<raw_ptr<Transaction, CtnExperimental>>;
  typedef std::list<std::unique_ptr<WorkItem>> WorkItemList;

  // We implement a basic reader/writer lock for the disk cache entry. If there
  // is a writer, then all transactions must wait to read the body. But the
  // waiting transactions can start their headers phase in parallel. Headers
  // phase is allowed for one transaction at a time so that if it doesn't match
  // the existing headers, remaining transactions do not also try to match the
  // existing entry in parallel leading to wasted network requests. If the
  // headers do not match, this entry will be doomed.
  //
  // A transaction goes through these state transitions.
  //
  // Write mode transactions eligible for shared writing:
  // add_to_entry_queue-> headers_transaction -> writers (first writer)
  // add_to_entry_queue-> headers_transaction -> done_headers_queue -> writers
  // (subsequent writers)
  // add_to_entry_queue-> headers_transaction -> done_headers_queue -> readers
  // (transactions not eligible for shared writing - once the data is written to
  // the cache by writers)
  //
  // Read only transactions:
  // add_to_entry_queue-> headers_transaction -> done_headers_queue -> readers
  // (once the data is written to the cache by writers)

  class NET_EXPORT_PRIVATE ActiveEntry : public base::RefCounted<ActiveEntry> {
   public:
    ActiveEntry(base::WeakPtr<HttpCache> cache,
                disk_cache::Entry* entry,
                bool opened_in);

    ActiveEntry(ActiveEntry const&) = delete;
    ActiveEntry& operator=(ActiveEntry const&) = delete;

    disk_cache::Entry* GetEntry() { return disk_entry_.get(); }

    bool opened() const { return opened_; }

    void set_opened(bool opened) { opened_ = opened; }

    bool will_process_queued_transactions() {
      return will_process_queued_transactions_;
    }

    void set_will_process_queued_transactions(
        bool will_process_queued_transactions) {
      will_process_queued_transactions_ = will_process_queued_transactions;
    }

    TransactionList& add_to_entry_queue() { return add_to_entry_queue_; }

    TransactionList& done_headers_queue() { return done_headers_queue_; }

    TransactionSet& readers() { return readers_; }

    const Transaction* headers_transaction() const {
      return headers_transaction_;
    }

    void ClearHeadersTransaction() { headers_transaction_ = nullptr; }

    bool HasWriters() const { return writers_.get(); }

    // Returns true if a transaction is currently writing the response body.
    bool IsWritingInProgress() const { return writers_.get(); }

    Writers* writers() const { return writers_.get(); }

    void Doom();

    bool IsDoomed() { return doomed_; }

    bool TransactionInReaders(Transaction* transaction) const;

    // Restarts headers_transaction and done_headers_queue transactions.
    void RestartHeadersPhaseTransactions();

    // Restarts the headers_transaction by setting its state. Since the
    // headers_transaction is awaiting an asynchronous operation completion,
    // it will be restarted when it's Cache IO callback is invoked.
    void RestartHeadersTransaction();

    // Checks if a transaction can be added to `add_to_entry_queue_`. If yes, it
    // will invoke the Cache IO callback of the transaction. It will take a
    // transaction from add_to_entry_queue and make it a headers_transaction, if
    // one doesn't exist already.
    void ProcessAddToEntryQueue();

    // Removes `transaction` from the `add_to_entry_queue_`.
    bool RemovePendingTransaction(Transaction* transaction);

    // Removes and returns all queued transactions in `this` in FIFO order.
    // This includes transactions that have completed the headers phase and
    // those that have not been added to the entry yet in that order.
    TransactionList TakeAllQueuedTransactions();

    void ReleaseWriters();

    void AddTransactionToWriters(
        Transaction* transaction,
        ParallelWritingPattern parallel_writing_pattern);

    // Returns true if this transaction can write headers to the entry.
    bool CanTransactionWriteResponseHeaders(Transaction* transaction,
                                            bool is_partial,
                                            bool is_match) const;

   private:
    friend class base::RefCounted<ActiveEntry>;

    ~ActiveEntry();

    // Destroys `this`.
    void Deactivate();

    // Destroys `this` using an exhaustive search.
    void SlowDeactivate();

    // Closes a previously doomed entry.
    void FinalizeDoomed();

    // The HttpCache that created this.
    base::WeakPtr<HttpCache> cache_;

    const disk_cache::ScopedEntryPtr disk_entry_;

    // Indicates if the disk_entry was opened or not (i.e.: created).
    // It is set to true when a transaction is added to an entry so that other,
    // queued, transactions do not mistake it for a newly created entry.
    bool opened_ = false;

    // Transactions waiting to be added to entry.
    TransactionList add_to_entry_queue_;

    // Transaction currently in the headers phase, either validating the
    // response or getting new headers. This can exist simultaneously with
    // writers or readers while validating existing headers.
    raw_ptr<Transaction> headers_transaction_ = nullptr;

    // Transactions that have completed their headers phase and are waiting
    // to read the response body or write the response body.
    TransactionList done_headers_queue_;

    // Transactions currently reading from the network and writing to the cache.
    std::unique_ptr<Writers> writers_;

    // Transactions that can only read from the cache. Only one of writers or
    // readers can be non-empty at a time.
    TransactionSet readers_;

    // The following variables are true if OnProcessQueuedTransactions is posted
    bool will_process_queued_transactions_ = false;

    // True if entry is doomed.
    bool doomed_ = false;
  };

  // `ActiveEntriesMap` and `ActiveEntriesSet` holding `raw_ref`s to
  // `ActiveEntry` is safe because `ActiveEntry` removes itself from the map or
  // set it is in on destruction.
  using ActiveEntriesMap =
      std::unordered_map<std::string, base::raw_ref<ActiveEntry>>;
  using PendingOpsMap =
      std::unordered_map<std::string, raw_ptr<PendingOp, CtnExperimental>>;
  using ActiveEntriesSet = std::set<base::raw_ref<ActiveEntry>>;

  // Methods ------------------------------------------------------------------

  // Returns whether a request can be cached. Certain types of requests can't or
  // shouldn't be cached, such as requests with a transient NetworkIsolationKey
  // (when network state partitioning is enabled) or requests with an opaque
  // initiator (for HTTP cache experiment partition schemes that incorporate the
  // initiator into the cache key).
  static bool CanGenerateCacheKeyForRequest(const HttpRequestInfo* request);

  // Generates a cache key given the various pieces used to construct the key.
  // Must not be called if a corresponding `CanGenerateCacheKeyForRequest`
  // returns false.
  static std::string GenerateCacheKey(
      const GURL& url,
      int load_flags,
      const NetworkIsolationKey& network_isolation_key,
      int64_t upload_data_identifier,
      bool is_subframe_document_resource,
      bool is_mainframe_navigation,
      std::optional<url::Origin> initiator);

  // Creates a WorkItem and sets it as the |pending_op|'s writer, or adds it to
  // the queue if a writer already exists.
  Error CreateAndSetWorkItem(scoped_refptr<ActiveEntry>* entry,
                             Transaction* transaction,
                             WorkItemOperation operation,
                             PendingOp* pending_op);

  // Creates the `disk_cache_` object and notifies the `callback` when the
  // operation completes. Returns an error code.
  int CreateBackend(CompletionOnceCallback callback);

  void ReportGetBackendResult(GetBackendCallback callback, int net_error);

  // Makes sure that the backend creation is complete before allowing the
  // provided transaction to use the object. Returns an error code.
  // |transaction| will be notified via its Cache IO callback if this method
  // returns ERR_IO_PENDING. The transaction is free to use the backend
  // directly at any time after receiving the notification.
  int GetBackendForTransaction(Transaction* transaction);

  // Dooms the entry selected by |key|, if it is currently in the list of active
  // entries.
  void DoomActiveEntry(const std::string& key);

  // Dooms the entry selected by |key|. |transaction| will be notified via its
  // Cache IO callback if this method returns ERR_IO_PENDING. The entry can be
  // currently in use or not. If entry is in use and the invoking transaction
  // is associated with this entry and this entry is already doomed, this API
  // should not be invoked.
  int DoomEntry(const std::string& key, Transaction* transaction);

  // Dooms the entry selected by |key|. |transaction| will be notified via its
  // Cache IO callback if this method returns ERR_IO_PENDING. The entry should
  // not be currently in use.
  int AsyncDoomEntry(const std::string& key, Transaction* transaction);

  // Dooms the entry associated with a GET for a given url and network
  // isolation key.
  void DoomMainEntryForUrl(const GURL& url,
                           const NetworkIsolationKey& isolation_key,
                           bool is_subframe_document_resource,
                           bool is_main_frame_navigation,
                           const std::optional<url::Origin>& initiator);

  // Returns if there is an entry that is currently in use and not doomed, or
  // NULL.
  bool HasActiveEntry(const std::string& key);

  // Returns an entry that is currently in use and not doomed, or NULL.
  scoped_refptr<ActiveEntry> GetActiveEntry(const std::string& key);

  // Creates a new ActiveEntry and starts tracking it. |disk_entry| is the disk
  // cache entry.
  scoped_refptr<ActiveEntry> ActivateEntry(disk_cache::Entry* disk_entry,
                                           bool opened);

  // Returns the PendingOp for the desired |key|. If an entry is not under
  // construction already, a new PendingOp structure is created.
  PendingOp* GetPendingOp(const std::string& key);

  // Deletes a PendingOp.
  void DeletePendingOp(PendingOp* pending_op);

  // Opens the disk cache entry associated with |key|, creating the entry if it
  // does not already exist, returning an ActiveEntry in |*entry|. |transaction|
  // will be notified via its Cache IO callback if this method returns
  // ERR_IO_PENDING. This should not be called if there already is an active
  // entry associated with |key|, e.g. you should call GetActiveEntry first.
  int OpenOrCreateEntry(const std::string& key,
                        scoped_refptr<ActiveEntry>* entry,
                        Transaction* transaction);

  // Opens the disk cache entry associated with |key|, returning an ActiveEntry
  // in |*entry|. |transaction| will be notified via its Cache IO callback if
  // this method returns ERR_IO_PENDING. This should not be called if there
  // already is an active entry associated with |key|, e.g. you should call
  // GetActiveEntry first.
  int OpenEntry(const std::string& key,
                scoped_refptr<ActiveEntry>* entry,
                Transaction* transaction);

  // Creates the disk cache entry associated with |key|, returning an
  // ActiveEntry in |*entry|. |transaction| will be notified via its Cache IO
  // callback if this method returns ERR_IO_PENDING.
  int CreateEntry(const std::string& key,
                  scoped_refptr<ActiveEntry>* entry,
                  Transaction* transaction);

  // Adds a transaction to an ActiveEntry. This method returns ERR_IO_PENDING
  // and the transaction will be notified about completion via a callback to
  // cache_io_callback().
  // In a failure case, the callback will be invoked with ERR_CACHE_RACE.
  int AddTransactionToEntry(scoped_refptr<ActiveEntry>& entry,
                            Transaction* transaction);

  // Transaction invokes this when its response headers phase is complete
  // If the transaction is responsible for writing the response body,
  // it becomes the writer and returns OK. In other cases ERR_IO_PENDING is
  // returned and the transaction will be notified about completion via its
  // Cache IO callback. In a failure case, the callback will be invoked with
  // ERR_CACHE_RACE.
  int DoneWithResponseHeaders(scoped_refptr<ActiveEntry>& entry,
                              Transaction* transaction,
                              bool is_partial);

  // Called when the transaction has finished working with this entry.
  // |entry_is_complete| is true if the transaction finished reading/writing
  // from the entry successfully, else it's false.
  void DoneWithEntry(scoped_refptr<ActiveEntry>& entry,
                     Transaction* transaction,
                     bool entry_is_complete,
                     bool is_partial);

  // Invoked when writers wants to doom the entry and restart any queued and
  // headers transactions.
  // Virtual so that it can be extended in tests.
  virtual void WritersDoomEntryRestartTransactions(ActiveEntry* entry);

  // Invoked when current transactions in writers have completed writing to the
  // cache. It may be successful completion of the response or failure as given
  // by |success|. Must delete the writers object.
  // |entry| is the owner of writers.
  // |should_keep_entry| indicates if the entry should be doomed/destroyed.
  // Virtual so that it can be extended in tests.
  virtual void WritersDoneWritingToEntry(scoped_refptr<ActiveEntry> entry,
                                         bool success,
                                         bool should_keep_entry,
                                         TransactionSet make_readers);

  // Called when the transaction has received a non-matching response to
  // validation and it's not the transaction responsible for writing the
  // response body.
  void DoomEntryValidationNoMatch(scoped_refptr<ActiveEntry> entry);

  // Processes either writer's failure to write response body or
  // headers_transactions's failure to write headers.
  void ProcessEntryFailure(ActiveEntry* entry);

  // Resumes processing the queued transactions of |entry|.
  void ProcessQueuedTransactions(scoped_refptr<ActiveEntry> entry);

  // Checks if a transaction can be added to the entry. If yes, it will
  // invoke the Cache IO callback of the transaction. This is a helper function
  // for OnProcessQueuedTransactions. It will take a transaction from
  // add_to_entry_queue and make it a headers_transaction, if one doesn't exist
  // already.
  void ProcessAddToEntryQueue(scoped_refptr<ActiveEntry> entry);

  // The implementation is split into a separate function so that it can be
  // called with a delay for testing.
  void ProcessAddToEntryQueueImpl(scoped_refptr<ActiveEntry> entry);

  // Returns if the transaction can join other transactions for writing to
  // the cache simultaneously. It is only supported for non-Read only,
  // GET requests which are not range requests.
  ParallelWritingPattern CanTransactionJoinExistingWriters(
      Transaction* transaction);

  // Invoked when a transaction that has already completed the response headers
  // phase can resume reading/writing the response body. It will invoke the IO
  // callback of the transaction. This is a helper function for
  // OnProcessQueuedTransactions.
  void ProcessDoneHeadersQueue(scoped_refptr<ActiveEntry> entry);

  // Returns the LoadState of the provided pending transaction.
  LoadState GetLoadStateForPendingTransaction(const Transaction* transaction);

  // Removes the transaction |transaction|, from the pending list of an entry
  // (PendingOp, active or doomed entry).
  void RemovePendingTransaction(Transaction* transaction);

  // Removes the transaction |transaction|, from the pending list of
  // |pending_op|.
  bool RemovePendingTransactionFromPendingOp(PendingOp* pending_op,
                                             Transaction* transaction);

  // Notes that `key` led to receiving a no-store response.
  // Stored keys rotate out in an LRU fashion so there is no guarantee that keys
  // are accounted for throughout the lifetime of this class.
  void MarkKeyNoStore(const std::string& key);

  // Returns true if the `key` is has probably led to a no-store response.
  // Collisions are possible so this function should not be used to make
  // decisions that affect correctness. A correct use is to use the information
  // to avoid attempting creating cache entries uselessly.
  bool DidKeyLeadToNoStoreResponse(const std::string& key);

  // Events (called via PostTask) ---------------------------------------------

  void OnProcessQueuedTransactions(scoped_refptr<ActiveEntry> entry);

  // Callbacks ----------------------------------------------------------------

  // Processes BackendCallback notifications.
  void OnIOComplete(int result, PendingOp* entry);

  // Helper to conditionally delete |pending_op| if HttpCache has been deleted.
  // This is necessary because |pending_op| owns a disk_cache::Backend that has
  // been passed in to CreateCacheBackend(), therefore must live until callback
  // is called.
  static void OnPendingOpComplete(base::WeakPtr<HttpCache> cache,
                                  PendingOp* pending_op,
                                  int result);

  // Variant for Open/Create method family, which has a different signature.
  static void OnPendingCreationOpComplete(base::WeakPtr<HttpCache> cache,
                                          PendingOp* pending_op,
                                          disk_cache::EntryResult result);

  // Variant for CreateCacheBackend, which has a different signature.
  static void OnPendingBackendCreationOpComplete(
      base::WeakPtr<HttpCache> cache,
      PendingOp* pending_op,
      disk_cache::BackendResult result);

  // Processes the backend creation notification.
  void OnBackendCreated(int result, PendingOp* pending_op);

  // Constants ----------------------------------------------------------------

  // Used when generating and accessing keys if cache is split.
  static const char kDoubleKeyPrefix[];
  static const char kDoubleKeySeparator[];
  static const char kSubframeDocumentResourcePrefix[];

  // Used for single-keyed entries if the cache is split.
  static const char kSingleKeyPrefix[];
  static const char kSingleKeySeparator[];

  // Variables ----------------------------------------------------------------

  raw_ptr<NetLog> net_log_;

  // Used when lazily constructing the disk_cache_.
  std::unique_ptr<BackendFactory> backend_factory_;
  bool building_backend_ = false;
  bool bypass_lock_for_test_ = false;
  bool bypass_lock_after_headers_for_test_ = false;
  bool delay_add_transaction_to_entry_for_test_ = false;
  bool fail_conditionalization_for_test_ = false;

  Mode mode_ = NORMAL;

  std::unique_ptr<HttpTransactionFactory> network_layer_;

  std::unique_ptr<disk_cache::Backend> disk_cache_;

  // The set of active entries indexed by cache key.
  ActiveEntriesMap active_entries_;

  // The set of doomed entries.
  ActiveEntriesSet doomed_entries_;

  // The set of entries "under construction".
  PendingOpsMap pending_ops_;

  // A clock that can be swapped out for testing.
  raw_ptr<base::Clock> clock_;

  // Used to track which keys led to a no-store response.
  base::LRUCacheSet<base::SHA1Digest> keys_marked_no_store_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<HttpCache> weak_factory_{this};
};

}  // namespace net

#endif  // NET_HTTP_HTTP_CACHE_H_
