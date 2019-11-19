// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares HttpCache::Transaction, a private class of HttpCache so
// it should only be included by http_cache.cc

#ifndef NET_HTTP_HTTP_CACHE_TRANSACTION_H_
#define NET_HTTP_HTTP_CACHE_TRANSACTION_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_states.h"
#include "net/base/net_error_details.h"
#include "net/base/request_priority.h"
#include "net/http/http_cache.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/http/partial_data.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/connection_attempts.h"
#include "net/websockets/websocket_handshake_stream_base.h"

namespace net {

class PartialData;
struct HttpRequestInfo;
struct LoadTimingInfo;
class SSLPrivateKey;

// This is the transaction that is returned by the HttpCache transaction
// factory.
class NET_EXPORT_PRIVATE HttpCache::Transaction : public HttpTransaction {
 public:
  // The transaction has the following modes, which apply to how it may access
  // its cache entry.
  //
  //  o If the mode of the transaction is NONE, then it is in "pass through"
  //    mode and all methods just forward to the inner network transaction.
  //
  //  o If the mode of the transaction is only READ, then it may only read from
  //    the cache entry.
  //
  //  o If the mode of the transaction is only WRITE, then it may only write to
  //    the cache entry.
  //
  //  o If the mode of the transaction is READ_WRITE, then the transaction may
  //    optionally modify the cache entry (e.g., possibly corresponding to
  //    cache validation).
  //
  //  o If the mode of the transaction is UPDATE, then the transaction may
  //    update existing cache entries, but will never create a new entry or
  //    respond using the entry read from the cache.
  enum Mode {
    NONE            = 0,
    READ_META       = 1 << 0,
    READ_DATA       = 1 << 1,
    READ            = READ_META | READ_DATA,
    WRITE           = 1 << 2,
    READ_WRITE      = READ | WRITE,
    UPDATE          = READ_META | WRITE,  // READ_WRITE & ~READ_DATA
  };

  // This enum backs a histogram. Please keep enums.xml up to date with any
  // changes, and new entries should be appended at the end. Never re-arrange /
  // re-use values.
  enum class NetworkIsolationKeyPresent {
    kNotPresentCacheableRequest = 0,
    kNotPresentNonCacheableRequest = 1,
    kPresent = 2,
    kMaxValue = kPresent,
  };

  Transaction(RequestPriority priority,
              HttpCache* cache);
  ~Transaction() override;

  // Virtual so it can be extended for testing.
  virtual Mode mode() const;

  std::string& method() { return method_; }

  const std::string& key() const { return cache_key_; }

  HttpCache::ActiveEntry* entry() { return entry_; }

  // Returns the LoadState of the writer transaction of a given ActiveEntry. In
  // other words, returns the LoadState of this transaction without asking the
  // http cache, because this transaction should be the one currently writing
  // to the cache entry.
  LoadState GetWriterLoadState() const;

  const CompletionRepeatingCallback& io_callback() { return io_callback_; }

  void SetIOCallBackForTest(CompletionRepeatingCallback cb) {
    io_callback_ = cb;
  }

  const NetLogWithSource& net_log() const;

  // Bypasses the cache lock whenever there is lock contention.
  void BypassLockForTest() {
    bypass_lock_for_test_ = true;
  }

  void BypassLockAfterHeadersForTest() {
    bypass_lock_after_headers_for_test_ = true;
  }

  // Generates a failure when attempting to conditionalize a network request.
  void FailConditionalizationForTest() {
    fail_conditionalization_for_test_ = true;
  }

  // HttpTransaction methods:
  int Start(const HttpRequestInfo* request_info,
            CompletionOnceCallback callback,
            const NetLogWithSource& net_log) override;
  int RestartIgnoringLastError(CompletionOnceCallback callback) override;
  int RestartWithCertificate(scoped_refptr<X509Certificate> client_cert,
                             scoped_refptr<SSLPrivateKey> client_private_key,
                             CompletionOnceCallback callback) override;
  int RestartWithAuth(const AuthCredentials& credentials,
                      CompletionOnceCallback callback) override;
  bool IsReadyToRestartForAuth() override;
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  void StopCaching() override;
  int64_t GetTotalReceivedBytes() const override;
  int64_t GetTotalSentBytes() const override;
  void DoneReading() override;
  const HttpResponseInfo* GetResponseInfo() const override;
  LoadState GetLoadState() const override;
  void SetQuicServerInfo(QuicServerInfo* quic_server_info) override;
  bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const override;
  bool GetRemoteEndpoint(IPEndPoint* endpoint) const override;
  void PopulateNetErrorDetails(NetErrorDetails* details) const override;
  void SetPriority(RequestPriority priority) override;
  void SetWebSocketHandshakeStreamCreateHelper(
      WebSocketHandshakeStreamBase::CreateHelper* create_helper) override;
  void SetBeforeNetworkStartCallback(
      const BeforeNetworkStartCallback& callback) override;
  void SetBeforeHeadersSentCallback(
      const BeforeHeadersSentCallback& callback) override;
  void SetRequestHeadersCallback(RequestHeadersCallback callback) override;
  void SetResponseHeadersCallback(ResponseHeadersCallback callback) override;
  int ResumeNetworkStart() override;
  void GetConnectionAttempts(ConnectionAttempts* out) const override;

  // Invoked when parallel validation cannot proceed due to response failure
  // and this transaction needs to be restarted.
  void SetValidatingCannotProceed();

  // Invoked to remove the association between a transaction waiting to be
  // added to an entry and the entry.
  void ResetCachePendingState() { cache_pending_ = false; }

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

  RequestPriority priority() const { return priority_; }
  PartialData* partial() { return partial_.get(); }
  bool is_truncated() { return truncated_; }

  // Invoked when this writer transaction is about to be removed from entry.
  // If result is an error code, a future Read should fail with |result|.
  void WriterAboutToBeRemovedFromEntry(int result);

  // Invoked when this transaction is about to become a reader because the cache
  // entry has finished writing.
  void WriteModeTransactionAboutToBecomeReader();

  // Invoked when HttpCache decides whether this transaction should join
  // parallel writing or create a new writers object. This is then used
  // for logging metrics. Can be called repeatedly, but doesn't change once the
  // value has been set to something other than PARALLEL_WRITING_NONE.
  void MaybeSetParallelWritingPatternForMetrics(ParallelWritingPattern pattern);

 private:
  static const size_t kNumValidationHeaders = 2;
  // Helper struct to pair a header name with its value, for
  // headers used to validate cache entries.
  struct ValidationHeaders {
    ValidationHeaders() : initialized(false) {}

    std::string values[kNumValidationHeaders];
    void Reset() {
      initialized = false;
      for (auto& value : values)
        value.clear();
    }
    bool initialized;
  };

  struct NetworkTransactionInfo {
    NetworkTransactionInfo();
    ~NetworkTransactionInfo();

    // Load timing information for the last network request, if any. Set in the
    // 304 and 206 response cases, as the network transaction may be destroyed
    // before the caller requests load timing information.
    std::unique_ptr<LoadTimingInfo> old_network_trans_load_timing;
    int64_t total_received_bytes = 0;
    int64_t total_sent_bytes = 0;
    ConnectionAttempts old_connection_attempts;
    IPEndPoint old_remote_endpoint;

    DISALLOW_COPY_AND_ASSIGN(NetworkTransactionInfo);
  };

  enum State {
    STATE_UNSET,

    // Normally, states are traversed in approximately this order.
    STATE_NONE,
    STATE_GET_BACKEND,
    STATE_GET_BACKEND_COMPLETE,
    STATE_INIT_ENTRY,
    STATE_OPEN_OR_CREATE_ENTRY,
    STATE_OPEN_OR_CREATE_ENTRY_COMPLETE,
    STATE_DOOM_ENTRY,
    STATE_DOOM_ENTRY_COMPLETE,
    STATE_CREATE_ENTRY,
    STATE_CREATE_ENTRY_COMPLETE,
    STATE_ADD_TO_ENTRY,
    STATE_ADD_TO_ENTRY_COMPLETE,
    STATE_DONE_HEADERS_ADD_TO_ENTRY_COMPLETE,
    STATE_CACHE_READ_RESPONSE,
    STATE_CACHE_READ_RESPONSE_COMPLETE,
    STATE_WRITE_UPDATED_PREFETCH_RESPONSE,
    STATE_WRITE_UPDATED_PREFETCH_RESPONSE_COMPLETE,
    STATE_CACHE_DISPATCH_VALIDATION,
    STATE_CACHE_QUERY_DATA,
    STATE_CACHE_QUERY_DATA_COMPLETE,
    STATE_START_PARTIAL_CACHE_VALIDATION,
    STATE_COMPLETE_PARTIAL_CACHE_VALIDATION,
    STATE_CACHE_UPDATE_STALE_WHILE_REVALIDATE_TIMEOUT,
    STATE_CACHE_UPDATE_STALE_WHILE_REVALIDATE_TIMEOUT_COMPLETE,
    STATE_SETUP_ENTRY_FOR_READ,
    STATE_SEND_REQUEST,
    STATE_SEND_REQUEST_COMPLETE,
    STATE_SUCCESSFUL_SEND_REQUEST,
    STATE_UPDATE_CACHED_RESPONSE,
    STATE_CACHE_WRITE_UPDATED_RESPONSE,
    STATE_CACHE_WRITE_UPDATED_RESPONSE_COMPLETE,
    STATE_UPDATE_CACHED_RESPONSE_COMPLETE,
    STATE_OVERWRITE_CACHED_RESPONSE,
    STATE_CACHE_WRITE_RESPONSE,
    STATE_CACHE_WRITE_RESPONSE_COMPLETE,
    STATE_TRUNCATE_CACHED_DATA,
    STATE_TRUNCATE_CACHED_DATA_COMPLETE,
    STATE_TRUNCATE_CACHED_METADATA,
    STATE_TRUNCATE_CACHED_METADATA_COMPLETE,
    STATE_PARTIAL_HEADERS_RECEIVED,
    STATE_HEADERS_PHASE_CANNOT_PROCEED,
    STATE_FINISH_HEADERS,
    STATE_FINISH_HEADERS_COMPLETE,

    // These states are entered from Read.
    STATE_NETWORK_READ_CACHE_WRITE,
    STATE_NETWORK_READ_CACHE_WRITE_COMPLETE,
    STATE_CACHE_READ_DATA,
    STATE_CACHE_READ_DATA_COMPLETE,
    // These states are entered if the request should be handled exclusively
    // by the network layer (skipping the cache entirely).
    STATE_NETWORK_READ,
    STATE_NETWORK_READ_COMPLETE,
  };

  // Used for categorizing validation triggers in histograms.
  // NOTE: This enumeration is used in histograms, so please do not add entries
  // in the middle.
  enum ValidationCause {
    VALIDATION_CAUSE_UNDEFINED,
    VALIDATION_CAUSE_VARY_MISMATCH,
    VALIDATION_CAUSE_VALIDATE_FLAG,
    VALIDATION_CAUSE_STALE,
    VALIDATION_CAUSE_ZERO_FRESHNESS,
    VALIDATION_CAUSE_MAX
  };

  enum MemoryEntryDataHints {
    // If this hint is set, the caching headers indicate we can't do anything
    // with this entry (unless we are ignoring them thanks to a loadflag),
    // i.e. it's expired and has nothing that permits validations.
    HINT_UNUSABLE_PER_CACHING_HEADERS = (1 << 0),
  };

  // Runs the state transition loop. Resets and calls |callback_| on exit,
  // unless the return value is ERR_IO_PENDING.
  int DoLoop(int result);

  // Each of these methods corresponds to a State value.  If there is an
  // argument, the value corresponds to the return of the previous state or
  // corresponding callback.
  int DoGetBackend();
  int DoGetBackendComplete(int result);
  int DoInitEntry();
  int DoOpenOrCreateEntry();
  int DoOpenOrCreateEntryComplete(int result);
  int DoDoomEntry();
  int DoDoomEntryComplete(int result);
  int DoCreateEntry();
  int DoCreateEntryComplete(int result);
  int DoAddToEntry();
  int DoAddToEntryComplete(int result);
  int DoDoneHeadersAddToEntryComplete(int result);
  int DoCacheReadResponse();
  int DoCacheReadResponseComplete(int result);
  int DoCacheWriteUpdatedPrefetchResponse(int result);
  int DoCacheWriteUpdatedPrefetchResponseComplete(int result);
  int DoCacheDispatchValidation();
  int DoCacheQueryData();
  int DoCacheQueryDataComplete(int result);
  int DoCacheUpdateStaleWhileRevalidateTimeout();
  int DoCacheUpdateStaleWhileRevalidateTimeoutComplete(int result);
  int DoSetupEntryForRead();
  int DoStartPartialCacheValidation();
  int DoCompletePartialCacheValidation(int result);
  int DoSendRequest();
  int DoSendRequestComplete(int result);
  int DoSuccessfulSendRequest();
  int DoUpdateCachedResponse();
  int DoCacheWriteUpdatedResponse();
  int DoCacheWriteUpdatedResponseComplete(int result);
  int DoUpdateCachedResponseComplete(int result);
  int DoOverwriteCachedResponse();
  int DoCacheWriteResponse();
  int DoCacheWriteResponseComplete(int result);
  int DoTruncateCachedData();
  int DoTruncateCachedDataComplete(int result);
  int DoTruncateCachedMetadata();
  int DoTruncateCachedMetadataComplete(int result);
  int DoPartialHeadersReceived();
  int DoHeadersPhaseCannotProceed(int result);
  int DoFinishHeaders(int result);
  int DoFinishHeadersComplete(int result);
  int DoNetworkReadCacheWrite();
  int DoNetworkReadCacheWriteComplete(int result);
  int DoCacheReadData();
  int DoCacheReadDataComplete(int result);
  int DoNetworkRead();
  int DoNetworkReadComplete(int result);

  // Adds time out handling while waiting to be added to entry or after headers
  // phase is complete.
  void AddCacheLockTimeoutHandler(ActiveEntry* entry);

  // Sets request_ and fields derived from it.
  void SetRequest(const NetLogWithSource& net_log);

  // Returns true if the request should be handled exclusively by the network
  // layer (skipping the cache entirely).
  bool ShouldPassThrough();

  // Called to begin reading from the cache.  Returns network error code.
  int BeginCacheRead();

  // Called to begin validating the cache entry.  Returns network error code.
  int BeginCacheValidation();

  // Called to begin validating an entry that stores partial content.  Returns
  // a network error code.
  int BeginPartialCacheValidation();

  // Validates the entry headers against the requested range and continues with
  // the validation of the rest of the entry.  Returns a network error code.
  int ValidateEntryHeadersAndContinue();

  // Called to start requests which were given an "if-modified-since" or
  // "if-none-match" validation header by the caller (NOT when the request was
  // conditionalized internally in response to LOAD_VALIDATE_CACHE).
  // Returns a network error code.
  int BeginExternallyConditionalizedRequest();

  // Called to restart a network transaction after an error.  Returns network
  // error code.
  int RestartNetworkRequest();

  // Called to restart a network transaction with a client certificate.
  // Returns network error code.
  int RestartNetworkRequestWithCertificate(
      scoped_refptr<X509Certificate> client_cert,
      scoped_refptr<SSLPrivateKey> client_private_key);

  // Called to restart a network transaction with authentication credentials.
  // Returns network error code.
  int RestartNetworkRequestWithAuth(const AuthCredentials& credentials);

  // Called to determine if we need to validate the cache entry before using it,
  // and whether the validation should be synchronous or asynchronous.
  ValidationType RequiresValidation();

  // Called to make the request conditional (to ask the server if the cached
  // copy is valid).  Returns true if able to make the request conditional.
  bool ConditionalizeRequest();

  // Determines if saved response permits conditionalization, and extracts
  // etag/last-modified values. Only depends on |response_.headers|.
  // |*etag_value| and |*last_modified_value| will be set if true is returned,
  // but may also be modified in other cases.
  bool IsResponseConditionalizable(std::string* etag_value,
                                   std::string* last_modified_value) const;

  // Returns true if |method_| indicates that we should only try to open an
  // entry and not attempt to create.
  bool ShouldOpenOnlyMethods() const;

  // Returns true if the resource info MemoryEntryDataHints bit flags in
  // |in_memory_info| and the current request & load flags suggest that
  // the cache entry in question is not actually usable for HTTP
  // (i.e. already expired, and nothing is forcing us to disregard that).
  bool MaybeRejectBasedOnEntryInMemoryData(uint8_t in_memory_info);

  // Returns true if response_ is such that, if saved to cache, it would only
  // be usable if load flags asked us to ignore caching headers.
  // (return value of false makes no statement as to suitability of the entry).
  bool ComputeUnusablePerCachingHeaders();

  // Makes sure that a 206 response is expected.  Returns true on success.
  // On success, handling_206_ will be set to true if we are processing a
  // partial entry.
  bool ValidatePartialResponse();

  // Handles a response validation error by bypassing the cache.
  void IgnoreRangeRequest();

  // Fixes the response headers to match expectations for a HEAD request.
  void FixHeadersForHead();

  // Called to write data to the cache entry.  If the write fails, then the
  // cache entry is destroyed.  Future calls to this function will just do
  // nothing without side-effect.  Returns a network error code.
  int WriteToEntry(int index,
                   int offset,
                   IOBuffer* data,
                   int data_len,
                   CompletionOnceCallback callback);

  // Called to write a response to the cache entry. |truncated| indicates if the
  // entry should be marked as incomplete.
  int WriteResponseInfoToEntry(const HttpResponseInfo& response,
                               bool truncated);

  // Helper function, should be called with result of WriteResponseInfoToEntry
  // (or the result of the callback, when WriteResponseInfoToEntry returns
  // ERR_IO_PENDING). Calls DoneWithEntry if |result| is not the right
  // number of bytes. It is expected that the state that calls this will
  // return whatever net error code this function returns, which currently
  // is always "OK".
  int OnWriteResponseInfoToEntryComplete(int result);

  // Configures the transaction to read from the network and stop writing to the
  // entry. It will release the entry if possible. Returns true if caching could
  // be stopped successfully. It will not be stopped if there are multiple
  // transactions writing to the cache simultaneously.
  bool StopCachingImpl(bool success);

  // Informs the HttpCache that this transaction is done with the entry and
  // changes the mode to NONE. Set |entry_is_complete| to false if the
  // transaction has not yet finished fully writing or reading the request
  // to/from the entry. If |entry_is_complete| is false the result may be either
  // a truncated or a doomed entry based on whether the stored response can be
  // resumed or not.
  void DoneWithEntry(bool did_finish);

  // Returns an error to signal the caller that the current read failed. The
  // current operation |result| is also logged. If |restart| is true, the
  // transaction should be restarted.
  int OnCacheReadError(int result, bool restart);

  // Called when the cache lock timeout fires.
  void OnCacheLockTimeout(base::TimeTicks start_time);

  // Deletes the current partial cache entry (sparse), and optionally removes
  // the control object (partial_).
  void DoomPartialEntry(bool delete_object);

  // Performs the needed work after receiving data from the network, when
  // working with range requests.
  int DoPartialNetworkReadCompleted(int result);

  // Performs the needed work after receiving data from the cache, when
  // working with range requests.
  int DoPartialCacheReadCompleted(int result);

  // Restarts this transaction after deleting the cached data. It is meant to
  // be used when the current request cannot be fulfilled due to conflicts
  // between the byte range request and the cached entry.
  int DoRestartPartialRequest();

  // Resets the relavant internal state to remove traces of internal processing
  // related to range requests. Deletes |partial_| if |delete_object| is true.
  void ResetPartialState(bool delete_object);

  // Resets |network_trans_|, which must be non-NULL.  Also updates
  // |old_network_trans_load_timing_|, which must be NULL when this is called.
  void ResetNetworkTransaction();

  // Returns the currently active network transaction.
  const HttpTransaction* network_transaction() const;
  HttpTransaction* network_transaction();

  // Returns the network transaction from |this| or from writers only if it was
  // moved from |this| to writers. This is so that statistics of the network
  // transaction are not attributed to any other writer member.
  const HttpTransaction* GetOwnedOrMovedNetworkTransaction() const;

  // Returns true if we should bother attempting to resume this request if it is
  // aborted while in progress. If |has_data| is true, the size of the stored
  // data is considered for the result.
  bool CanResume(bool has_data);

  // Setter for response_ and auth_response_. It updates its cache entry status,
  // if needed.
  void SetResponse(const HttpResponseInfo& new_response);
  void SetAuthResponse(const HttpResponseInfo& new_response);

  void UpdateCacheEntryStatus(
      HttpResponseInfo::CacheEntryStatus new_cache_entry_status);

  // Sets the response.cache_entry_status to the current cache_entry_status_.
  void SyncCacheEntryStatusToResponse();

  // Logs histograms for this transaction. It is invoked when the transaction is
  // either complete or is done writing to entry and will continue in
  // network-only mode.
  void RecordHistograms();

  // Returns true if this transaction is a member of entry_->writers.
  bool InWriters() const;

  // Called to signal completion of asynchronous IO. Note that this callback is
  // used in the conventional sense where one layer calls the callback of the
  // layer above it e.g. this callback gets called from the network transaction
  // layer. In addition, it is also used for HttpCache layer to let this
  // transaction know when it is out of a queued state in ActiveEntry and can
  // continue its processing.
  void OnIOComplete(int result);

  // When in a DoLoop, use this to set the next state as it verifies that the
  // state isn't set twice.
  void TransitionToState(State state);

  // Helper function to decide the next reading state.
  int TransitionToReadingState();

  // Saves network transaction info using |transaction|.
  void SaveNetworkTransactionInfo(const HttpTransaction& transaction);

  // Disables caching for media content when running on battery.
  bool ShouldDisableMediaCaching(const HttpResponseHeaders* headers) const;

  State next_state_;

  // Initial request with which Start() was invoked.
  const HttpRequestInfo* initial_request_;

  const HttpRequestInfo* request_;

  std::string method_;
  RequestPriority priority_;
  NetLogWithSource net_log_;
  std::unique_ptr<HttpRequestInfo> custom_request_;
  HttpRequestHeaders request_headers_copy_;
  // If extra_headers specified a "if-modified-since" or "if-none-match",
  // |external_validation_| contains the value of those headers.
  ValidationHeaders external_validation_;
  base::WeakPtr<HttpCache> cache_;
  HttpCache::ActiveEntry* entry_;
  HttpCache::ActiveEntry* new_entry_;
  std::unique_ptr<HttpTransaction> network_trans_;
  CompletionOnceCallback callback_;  // Consumer's callback.
  HttpResponseInfo response_;
  HttpResponseInfo auth_response_;

  // This is only populated when we want to modify a prefetch request in some
  // way for future transactions, while leaving it untouched for the current
  // one. DoCacheReadResponseComplete() sets this to a copy of |response_|,
  // and modifies the members for future transactions. Then,
  // WriteResponseInfoToEntry() writes |updated_prefetch_response_| to the cache
  // entry if it is populated, or |response_| otherwise. Finally,
  // WriteResponseInfoToEntry() resets this to base::nullopt.
  std::unique_ptr<HttpResponseInfo> updated_prefetch_response_;

  const HttpResponseInfo* new_response_;
  std::string cache_key_;
  Mode mode_;
  bool reading_;  // We are already reading. Never reverts to false once set.
  bool invalid_range_;  // We may bypass the cache for this request.
  bool truncated_;  // We don't have all the response data.
  bool is_sparse_;  // The data is stored in sparse byte ranges.
  bool range_requested_;  // The user requested a byte range.
  bool handling_206_;  // We must deal with this 206 response.
  bool cache_pending_;  // We are waiting for the HttpCache.

  // Headers have been received from the network and it's not a match with the
  // existing entry.
  bool done_headers_create_new_entry_;

  bool vary_mismatch_;  // The request doesn't match the stored vary data.
  bool couldnt_conditionalize_request_;
  bool bypass_lock_for_test_;  // A test is exercising the cache lock.
  bool bypass_lock_after_headers_for_test_;  // A test is exercising the cache
                                             // lock.
  bool fail_conditionalization_for_test_;  // Fail ConditionalizeRequest.
  scoped_refptr<IOBuffer> read_buf_;

  // Length of the buffer passed in Read().
  int read_buf_len_;

  int io_buf_len_;
  int read_offset_;
  int effective_load_flags_;
  std::unique_ptr<PartialData> partial_;  // We are dealing with range requests.
  CompletionRepeatingCallback io_callback_;

  // Error code to be returned from a subsequent Read call if shared writing
  // failed in a separate transaction.
  int shared_writing_error_;

  // Members used to track data for histograms.
  // This cache_entry_status_ takes precedence over
  // response_.cache_entry_status. In fact, response_.cache_entry_status must be
  // kept in sync with cache_entry_status_ (via SetResponse and
  // UpdateCacheEntryStatus).
  HttpResponseInfo::CacheEntryStatus cache_entry_status_;
  ValidationCause validation_cause_;
  base::TimeTicks entry_lock_waiting_since_;
  base::TimeTicks first_cache_access_since_;
  base::TimeTicks send_request_since_;
  base::TimeTicks read_headers_since_;
  base::Time open_entry_last_used_;
  bool cant_conditionalize_zero_freshness_from_memhint_;
  bool recorded_histograms_;
  ParallelWritingPattern parallel_writing_pattern_;

  NetworkTransactionInfo network_transaction_info_;

  // True if this transaction created the network transaction that is now being
  // used by writers. This is used to check that only this transaction should
  // account for the network bytes and other statistics of the network
  // transaction.
  // TODO(shivanisha) Note that if this transaction dies mid-way and there are
  // other writer transactions, no transaction then accounts for those
  // statistics.
  bool moved_network_transaction_to_writers_;

  // The helper object to use to create WebSocketHandshakeStreamBase
  // objects. Only relevant when establishing a WebSocket connection.
  // This is passed to the underlying network transaction. It is stored here in
  // case the transaction does not exist yet.
  WebSocketHandshakeStreamBase::CreateHelper*
      websocket_handshake_stream_base_create_helper_;

  BeforeNetworkStartCallback before_network_start_callback_;
  BeforeHeadersSentCallback before_headers_sent_callback_;
  RequestHeadersCallback request_headers_callback_;
  ResponseHeadersCallback response_headers_callback_;

  // True if the Transaction is currently processing the DoLoop.
  bool in_do_loop_;

  base::WeakPtrFactory<Transaction> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Transaction);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_CACHE_TRANSACTION_H_
