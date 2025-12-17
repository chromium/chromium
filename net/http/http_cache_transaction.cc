// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_cache_transaction.h"

#include "base/byte_count.h"
#include "build/build_config.h"  // For IS_POSIX

#if BUILDFLAG(IS_POSIX)
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/stack_allocated.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"  // For EqualsCaseInsensitiveASCII.
#include "base/task/single_thread_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/values.h"
#include "net/base/auth.h"
#include "net/base/features.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/task/task_runner.h"
#include "net/base/trace_constants.h"
#include "net/base/transport_info.h"
#include "net/base/upload_data_stream.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/memory_entry_data_hints.h"
#include "net/http/http_cache.h"
#include "net/http/http_cache_util.h"
#include "net/http/http_cache_writers.h"
#include "net/http/http_log_util.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/http/no_vary_search_cache.h"
#include "net/log/net_log_event_type.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_config_service.h"

using base::Time;
using base::TimeTicks;

namespace net {

using CacheEntryStatus = HttpResponseInfo::CacheEntryStatus;

namespace {

constexpr base::TimeDelta kStaleRevalidateTimeout = base::Seconds(60);

// From http://tools.ietf.org/html/draft-ietf-httpbis-p6-cache-21#section-6
//      a "non-error response" is one with a 2xx (Successful) or 3xx
//      (Redirection) status code.
bool NonErrorResponse(int status_code) {
  int status_code_range = status_code / 100;
  return status_code_range == 2 || status_code_range == 3;
}

enum ExternallyConditionalizedType {
  EXTERNALLY_CONDITIONALIZED_CACHE_REQUIRES_VALIDATION,
  EXTERNALLY_CONDITIONALIZED_CACHE_USABLE,
  EXTERNALLY_CONDITIONALIZED_MISMATCHED_VALIDATORS,
  EXTERNALLY_CONDITIONALIZED_MAX
};

bool ShouldByPassCacheForFirstPartySets(
    const std::optional<int64_t>& clear_at_run_id,
    const std::optional<int64_t>& written_at_run_id) {
  return clear_at_run_id.has_value() &&
         (!written_at_run_id.has_value() ||
          written_at_run_id.value() < clear_at_run_id.value());
}

// Methods other than "GET" or "HEAD" can have request bodies, which causes
// problems for the request matching.
// TODO(https://crbug.com/390459312): Consider supporting additional methods.
bool MethodUsesNoVarySearch(const std::string& method) {
  return method == "GET" || method == "HEAD";
}

const scoped_refptr<base::SingleThreadTaskRunner>& TaskRunner(
    net::RequestPriority priority) {
  if (features::kNetTaskSchedulerHttpCacheTransaction.Get()) {
    return net::GetTaskRunner(priority);
  }
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

}  // namespace

#define CACHE_STATUS_HISTOGRAMS(type)                                      \
  UMA_HISTOGRAM_ENUMERATION("HttpCache.Pattern" type, cache_entry_status_, \
                            CacheEntryStatus::ENTRY_MAX)

#define IS_NO_STORE_HISTOGRAMS(type, is_no_store) \
  base::UmaHistogramBoolean("HttpCache.IsNoStore" type, is_no_store)

//-----------------------------------------------------------------------------

HttpCache::Transaction::Transaction(RequestPriority priority, HttpCache* cache)
    : track_for_state_change_(base::trace_event::GetNextGlobalTraceId()),
      priority_(priority),
      cache_(cache->GetWeakPtr()),
      read_no_vary_search_cache_(cache->no_vary_search_cache_) {
  io_callback_ = base::BindRepeating(&Transaction::OnIOComplete,
                                     weak_factory_.GetWeakPtr());
  cache_io_callback_ = base::BindRepeating(&Transaction::OnCacheIOComplete,
                                           weak_factory_.GetWeakPtr());
}

HttpCache::Transaction::~Transaction() {
  TRACE_EVENT_END(TRACE_DISABLED_BY_DEFAULT("net"), track_for_state_change_);
  if (no_vary_search_cache_erase_handle_) {
    net_log_.EndEvent(
        NetLogEventType::HTTP_CACHE_USING_NO_VARY_SEARCH_CACHE_URL);
  }
  RecordHistograms();

  // We may have to issue another IO, but we should never invoke the callback_
  // after this point.
  callback_.Reset();

  if (cache_) {
    if (entry_) {
      DoneWithEntry(false /* entry_is_complete */);
    } else if (cache_pending_) {
      cache_->RemovePendingTransaction(this);
    }
  }
}

HttpCache::Transaction::Mode HttpCache::Transaction::mode() const {
  return mode_;
}

LoadState HttpCache::Transaction::GetWriterLoadState() const {
  const HttpTransaction* transaction = network_transaction();
  if (transaction) {
    return transaction->GetLoadState();
  }
  if (entry_ || !request_) {
    return LOAD_STATE_IDLE;
  }
  return LOAD_STATE_WAITING_FOR_CACHE;
}

const NetLogWithSource& HttpCache::Transaction::net_log() const {
  return net_log_;
}

int HttpCache::Transaction::Start(const HttpRequestInfo* request,
                                  CompletionOnceCallback callback,
                                  const NetLogWithSource& net_log) {
  DCHECK(request);
  DCHECK(request->IsConsistent());
  DCHECK(!callback.is_null());
  TRACE_EVENT_BEGIN(TRACE_DISABLED_BY_DEFAULT("net"),
                    "HttpCacheTransactionState", track_for_state_change_, "url",
                    request->url.spec());

  // Ensure that we only have one asynchronous call at a time.
  DCHECK(callback_.is_null());
  DCHECK(!reading_);
  DCHECK(!network_trans_.get());
  DCHECK(!entry_);
  DCHECK_EQ(next_state_, STATE_NONE);

  if (!cache_.get()) {
    return ERR_UNEXPECTED;
  }

  initial_request_ = request;
  SetRequest(net_log);

  // We have to wait until the backend is initialized so we start the SM.
  next_state_ = STATE_GET_BACKEND;
  int rv = DoLoop(OK);

  // Setting this here allows us to check for the existence of a callback_ to
  // determine if we are still inside Start.
  if (rv == ERR_IO_PENDING) {
    callback_ = std::move(callback);
  }

  return rv;
}

int HttpCache::Transaction::RestartIgnoringLastError(
    CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());

  // Ensure that we only have one asynchronous call at a time.
  DCHECK(callback_.is_null());

  if (!cache_.get()) {
    return ERR_UNEXPECTED;
  }

  int rv = RestartNetworkRequest();

  if (rv == ERR_IO_PENDING) {
    callback_ = std::move(callback);
  }

  return rv;
}

int HttpCache::Transaction::RestartWithCertificate(
    scoped_refptr<X509Certificate> client_cert,
    scoped_refptr<SSLPrivateKey> client_private_key,
    CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());

  // Ensure that we only have one asynchronous call at a time.
  DCHECK(callback_.is_null());

  if (!cache_.get()) {
    return ERR_UNEXPECTED;
  }

  int rv = RestartNetworkRequestWithCertificate(std::move(client_cert),
                                                std::move(client_private_key));

  if (rv == ERR_IO_PENDING) {
    callback_ = std::move(callback);
  }

  return rv;
}

int HttpCache::Transaction::RestartWithAuth(const AuthCredentials& credentials,
                                            CompletionOnceCallback callback) {
  DCHECK(auth_response_.headers.get());
  DCHECK(!callback.is_null());

  // Ensure that we only have one asynchronous call at a time.
  DCHECK(callback_.is_null());

  if (!cache_.get()) {
    return ERR_UNEXPECTED;
  }

  // Clear the intermediate response since we are going to start over.
  SetAuthResponse(HttpResponseInfo());

  int rv = RestartNetworkRequestWithAuth(credentials);

  if (rv == ERR_IO_PENDING) {
    callback_ = std::move(callback);
  }

  return rv;
}

bool HttpCache::Transaction::IsReadyToRestartForAuth() {
  if (!network_trans_.get()) {
    return false;
  }
  return network_trans_->IsReadyToRestartForAuth();
}

int HttpCache::Transaction::Read(IOBuffer* buf,
                                 int buf_len,
                                 CompletionOnceCallback callback) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "Read",
                      track_for_state_change_, "buf_len", buf_len);

  DCHECK_EQ(next_state_, STATE_NONE);
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);
  DCHECK(!callback.is_null());

  DCHECK(callback_.is_null());

  if (!cache_.get()) {
    return ERR_UNEXPECTED;
  }

  // If we have an intermediate auth response at this point, then it means the
  // user wishes to read the network response (the error page).  If there is a
  // previous response in the cache then we should leave it intact.
  if (auth_response_.headers.get() && mode_ != NONE) {
    UpdateCacheEntryStatusToOther(OtherStatusReason::kReadingAuthResponse);
    DCHECK(mode_ & WRITE);
    bool stopped = StopCachingImpl(mode_ == READ_WRITE);
    DCHECK(stopped);
  }

  reading_ = true;
  read_buf_ = buf;
  read_buf_len_ = buf_len;
  int rv = TransitionToReadingState();
  if (rv != OK || next_state_ == STATE_NONE) {
    return rv;
  }

  rv = DoLoop(OK);

  if (rv == ERR_IO_PENDING) {
    DCHECK(callback_.is_null());
    callback_ = std::move(callback);
  }
  return rv;
}

int HttpCache::Transaction::TransitionToReadingState() {
  if (!entry_) {
    if (network_trans_) {
      // This can happen when the request should be handled exclusively by
      // the network layer (skipping the cache entirely using
      // LOAD_DISABLE_CACHE) or there was an error during the headers phase
      // due to which the transaction cannot write to the cache or the consumer
      // is reading the auth response from the network.
      // TODO(http://crbug.com/740947) to get rid of this state in future.
      next_state_ = STATE_NETWORK_READ;

      return OK;
    }

    // If there is no network, and no cache entry, then there is nothing to read
    // from.
    next_state_ = STATE_NONE;

    // An error state should be set for the next read, else this transaction
    // should have been terminated once it reached this state. To assert we
    // could dcheck that shared_writing_error_ is set to a valid error value but
    // in some specific conditions (http://crbug.com/806344) it's possible that
    // the consumer does an extra Read in which case the assert will fail.
    return shared_writing_error_;
  }

  // If entry_ is present, the transaction is either a member of entry_->writers
  // or readers.
  if (!InWriters()) {
    // Since transaction is not a writer and we are in Read(), it must be a
    // reader.
    DCHECK(entry_->TransactionInReaders(this));
    DCHECK(mode_ == READ || (mode_ == READ_WRITE && partial_));
    next_state_ = STATE_CACHE_READ_DATA;
    return OK;
  }

  DCHECK(mode_ & WRITE || mode_ == NONE);

  // If it's a writer and it is partial then it may need to read from the cache
  // or from the network based on whether network transaction is present or not.
  if (partial_) {
    if (entry_->writers()->network_transaction()) {
      next_state_ = STATE_NETWORK_READ_CACHE_WRITE;
    } else {
      next_state_ = STATE_CACHE_READ_DATA;
    }
    return OK;
  }

  // Full request.
  // If it's a writer and a full request then it may read from the cache if its
  // offset is behind the current offset else from the network.
  int disk_entry_size = entry_->GetEntry()->GetDataSize(kResponseContentIndex);
  if (read_offset_ == disk_entry_size ||
      entry_->writers()->network_read_only()) {
    next_state_ = STATE_NETWORK_READ_CACHE_WRITE;
  } else {
    DCHECK_LT(read_offset_, disk_entry_size);
    next_state_ = STATE_CACHE_READ_DATA;
  }
  return OK;
}

void HttpCache::Transaction::StopCaching() {
  // We really don't know where we are now. Hopefully there is no operation in
  // progress, but nothing really prevents this method to be called after we
  // returned ERR_IO_PENDING. We cannot attempt to truncate the entry at this
  // point because we need the state machine for that (and even if we are really
  // free, that would be an asynchronous operation). In other words, keep the
  // entry how it is (it will be marked as truncated at destruction), and let
  // the next piece of code that executes know that we are now reading directly
  // from the net.
  if (cache_.get() && (mode_ & WRITE) && !is_sparse_ && !range_requested_ &&
      network_transaction()) {
    StopCachingImpl(false);
  }
}

int64_t HttpCache::Transaction::GetTotalReceivedBytes() const {
  int64_t total_received_bytes = network_transaction_info_.total_received_bytes;
  const HttpTransaction* transaction = GetOwnedOrMovedNetworkTransaction();
  if (transaction) {
    total_received_bytes += transaction->GetTotalReceivedBytes();
  }
  return total_received_bytes;
}

int64_t HttpCache::Transaction::GetTotalSentBytes() const {
  int64_t total_sent_bytes = network_transaction_info_.total_sent_bytes;
  const HttpTransaction* transaction = GetOwnedOrMovedNetworkTransaction();
  if (transaction) {
    total_sent_bytes += transaction->GetTotalSentBytes();
  }
  return total_sent_bytes;
}

int64_t HttpCache::Transaction::GetReceivedBodyBytes() const {
  int64_t received_body_bytes = network_transaction_info_.received_body_bytes;
  const HttpTransaction* transaction = GetOwnedOrMovedNetworkTransaction();
  if (transaction) {
    received_body_bytes = transaction->GetReceivedBodyBytes();
  }
  return received_body_bytes;
}

void HttpCache::Transaction::DoneReading() {
  if (cache_.get() && entry_) {
    DCHECK_NE(mode_, UPDATE);
    DoneWithEntry(true);
  }
}

const HttpResponseInfo* HttpCache::Transaction::GetResponseInfo() const {
  // Null headers means we encountered an error or haven't a response yet
  if (auth_response_.headers.get()) {
    DCHECK_EQ(cache_entry_status_, auth_response_.cache_entry_status)
        << "These must be in sync via SetResponse and SetAuthResponse.";
    return &auth_response_;
  }
  // TODO(crbug.com/40772202): This should check in `response_`
  return &response_;
}

LoadState HttpCache::Transaction::GetLoadState() const {
  // If there's no pending callback, the ball is not in the
  // HttpCache::Transaction's court, whatever else may be going on.
  if (!callback_) {
    return LOAD_STATE_IDLE;
  }

  LoadState state = GetWriterLoadState();
  if (state != LOAD_STATE_WAITING_FOR_CACHE) {
    return state;
  }

  if (cache_.get()) {
    return cache_->GetLoadStateForPendingTransaction(this);
  }

  return LOAD_STATE_IDLE;
}

bool HttpCache::Transaction::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  const HttpTransaction* transaction = GetOwnedOrMovedNetworkTransaction();
  if (transaction) {
    return transaction->GetLoadTimingInfo(load_timing_info);
  }

  if (network_transaction_info_.old_network_trans_load_timing) {
    *load_timing_info =
        *network_transaction_info_.old_network_trans_load_timing;
    return true;
  }

  if (first_cache_access_since_.is_null()) {
    return false;
  }

  // If the cache entry was opened, return that time.
  load_timing_info->send_start = first_cache_access_since_;
  // This time doesn't make much sense when reading from the cache, so just use
  // the same time as send_start.
  load_timing_info->send_end = first_cache_access_since_;
  // Provide the time immediately before parsing a cached entry.
  load_timing_info->receive_headers_start = read_headers_since_;
  return true;
}

void HttpCache::Transaction::PopulateLoadTimingInternalInfo(
    LoadTimingInternalInfo* load_timing_internal_info) const {
  const HttpTransaction* transaction = GetOwnedOrMovedNetworkTransaction();
  if (transaction) {
    transaction->PopulateLoadTimingInternalInfo(load_timing_internal_info);
  }
}

bool HttpCache::Transaction::GetRemoteEndpoint(IPEndPoint* endpoint) const {
  const HttpTransaction* transaction = GetOwnedOrMovedNetworkTransaction();
  if (transaction) {
    return transaction->GetRemoteEndpoint(endpoint);
  }

  if (!network_transaction_info_.old_remote_endpoint.address().empty()) {
    *endpoint = network_transaction_info_.old_remote_endpoint;
    return true;
  }

  return false;
}

void HttpCache::Transaction::PopulateNetErrorDetails(
    NetErrorDetails* details) const {
  const HttpTransaction* transaction = GetOwnedOrMovedNetworkTransaction();
  if (transaction) {
    return transaction->PopulateNetErrorDetails(details);
  }
  return;
}

void HttpCache::Transaction::SetPriority(RequestPriority priority) {
  priority_ = priority;

  if (network_trans_) {
    network_trans_->SetPriority(priority_);
  }

  if (InWriters()) {
    DCHECK(!network_trans_ || partial_);
    entry_->writers()->UpdatePriority();
  }
}

void HttpCache::Transaction::SetWebSocketHandshakeStreamCreateHelper(
    WebSocketHandshakeStreamBase::CreateHelper* create_helper) {
  websocket_handshake_stream_base_create_helper_ = create_helper;

  // TODO(shivanisha). Since this function must be invoked before Start() as
  // per the API header, a network transaction should not exist at that point.
  HttpTransaction* transaction = network_transaction();
  if (transaction) {
    transaction->SetWebSocketHandshakeStreamCreateHelper(create_helper);
  }
}

void HttpCache::Transaction::SetConnectedCallback(
    const ConnectedCallback& callback) {
  DCHECK(!network_trans_);
  connected_callback_ = callback;
}

void HttpCache::Transaction::SetRequestHeadersCallback(
    RequestHeadersCallback callback) {
  DCHECK(!network_trans_);
  request_headers_callback_ = std::move(callback);
}

void HttpCache::Transaction::SetResponseHeadersCallback(
    ResponseHeadersCallback callback) {
  DCHECK(!network_trans_);
  response_headers_callback_ = std::move(callback);
}

void HttpCache::Transaction::SetEarlyResponseHeadersCallback(
    ResponseHeadersCallback callback) {
  DCHECK(!network_trans_);
  early_response_headers_callback_ = std::move(callback);
}

void HttpCache::Transaction::SetModifyRequestHeadersCallback(
    base::RepeatingCallback<void(HttpRequestHeaders*)> callback) {
  // This method should not be called for this class.
  NOTREACHED();
}

void HttpCache::Transaction::SetIsSharedDictionaryReadAllowedCallback(
    base::RepeatingCallback<bool()> callback) {
  DCHECK(!network_trans_);
  is_shared_dictionary_read_allowed_callback_ = std::move(callback);
}

ConnectionAttempts HttpCache::Transaction::GetConnectionAttempts() const {
  ConnectionAttempts attempts;
  const HttpTransaction* transaction = GetOwnedOrMovedNetworkTransaction();
  if (transaction) {
    attempts = transaction->GetConnectionAttempts();
  }

  attempts.insert(attempts.begin(),
                  network_transaction_info_.old_connection_attempts.begin(),
                  network_transaction_info_.old_connection_attempts.end());
  return attempts;
}

void HttpCache::Transaction::CloseConnectionOnDestruction() {
  if (network_trans_) {
    network_trans_->CloseConnectionOnDestruction();
  } else if (InWriters()) {
    entry_->writers()->CloseConnectionOnDestruction();
  }
}

void HttpCache::Transaction::SetValidatingCannotProceed() {
  DCHECK(!reading_);
  // Ensure this transaction is waiting for a callback.
  DCHECK_NE(STATE_UNSET, next_state_);

  next_state_ = STATE_HEADERS_PHASE_CANNOT_PROCEED;
  entry_.reset();
}

void HttpCache::Transaction::WriterAboutToBeRemovedFromEntry(int result) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "WriterAboutToBeRemovedFromEntry",
                      track_for_state_change_);
  // Since the transaction can no longer access the network transaction, save
  // all network related info now.
  if (moved_network_transaction_to_writers_ &&
      entry_->writers()->network_transaction()) {
    SaveNetworkTransactionInfo(*(entry_->writers()->network_transaction()));
  }

  entry_.reset();
  mode_ = NONE;

  // Transactions in the midst of a Read call through writers will get any error
  // code through the IO callback but for idle transactions/transactions reading
  // from the cache, the error for a future Read must be stored here.
  if (result < 0) {
    shared_writing_error_ = result;
  }
}

void HttpCache::Transaction::WriteModeTransactionAboutToBecomeReader() {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "WriteModeTransactionAboutToBecomeReader",
                      track_for_state_change_);
  mode_ = READ;
  if (moved_network_transaction_to_writers_ &&
      entry_->writers()->network_transaction()) {
    SaveNetworkTransactionInfo(*(entry_->writers()->network_transaction()));
  }
}

void HttpCache::Transaction::AddDiskCacheWriteTime(base::TimeDelta elapsed) {
  total_disk_cache_write_time_ += elapsed;
}

//-----------------------------------------------------------------------------

// A few common patterns: (Foo* means Foo -> FooComplete)
//
// 1. Not-cached entry:
//   Start():
//   GetBackend* -> InitEntry -> OpenOrCreateEntry* -> AddToEntry* ->
//   SendRequest* -> SuccessfulSendRequest -> OverwriteCachedResponse ->
//   CacheWriteResponse* -> TruncateCachedData* -> PartialHeadersReceived ->
//   FinishHeaders*
//
//   Read():
//   NetworkReadCacheWrite*/CacheReadData* (if other writers are also writing to
//   the cache)
//
// 2. Cached entry, no validation:
//   Start():
//   GetBackend* -> InitEntry -> OpenOrCreateEntry* -> AddToEntry* ->
//   CacheReadResponse* -> CacheDispatchValidation ->
//   BeginPartialCacheValidation() -> BeginCacheValidation() ->
//   ConnectedCallback* -> SetupEntryForRead() -> FinishHeaders*
//
//   Read():
//   CacheReadData*
//
// 3. Cached entry, validation (304):
//   Start():
//   GetBackend* -> InitEntry -> OpenOrCreateEntry* -> AddToEntry* ->
//   CacheReadResponse* -> CacheDispatchValidation ->
//   BeginPartialCacheValidation() -> BeginCacheValidation() -> SendRequest* ->
//   SuccessfulSendRequest -> UpdateCachedResponse -> CacheWriteUpdatedResponse*
//   -> UpdateCachedResponseComplete -> OverwriteCachedResponse ->
//   PartialHeadersReceived -> FinishHeaders*
//
//   Read():
//   CacheReadData*
//
// 4. Cached entry, validation and replace (200):
//   Start():
//   GetBackend* -> InitEntry -> OpenOrCreateEntry* -> AddToEntry* ->
//   CacheReadResponse* -> CacheDispatchValidation ->
//   BeginPartialCacheValidation() -> BeginCacheValidation() -> SendRequest* ->
//   SuccessfulSendRequest -> OverwriteCachedResponse -> CacheWriteResponse* ->
//   DoTruncateCachedData* -> PartialHeadersReceived -> FinishHeaders*
//
//   Read():
//   NetworkReadCacheWrite*/CacheReadData* (if other writers are also writing to
//   the cache)
//
// 5. Sparse entry, partially cached, byte range request:
//   Start():
//   GetBackend* -> InitEntry -> OpenOrCreateEntry* -> AddToEntry* ->
//   CacheReadResponse* -> CacheDispatchValidation ->
//   BeginPartialCacheValidation() -> CacheQueryData* ->
//   ValidateEntryHeadersAndContinue() -> StartPartialCacheValidation ->
//   CompletePartialCacheValidation -> BeginCacheValidation() -> SendRequest* ->
//   SuccessfulSendRequest -> UpdateCachedResponse -> CacheWriteUpdatedResponse*
//   -> UpdateCachedResponseComplete -> OverwriteCachedResponse ->
//   PartialHeadersReceived -> FinishHeaders*
//
//   Read() 1:
//   NetworkReadCacheWrite*
//
//   Read() 2:
//   NetworkReadCacheWrite* -> StartPartialCacheValidation ->
//   CompletePartialCacheValidation -> ConnectedCallback* -> CacheReadData*
//
//   Read() 3:
//   CacheReadData* -> StartPartialCacheValidation ->
//   CompletePartialCacheValidation -> BeginCacheValidation() -> SendRequest* ->
//   SuccessfulSendRequest -> UpdateCachedResponse* -> OverwriteCachedResponse
//   -> PartialHeadersReceived -> NetworkReadCacheWrite*
//
// 6. HEAD. Not-cached entry:
//   Pass through. Don't save a HEAD by itself.
//   Start():
//   GetBackend* -> InitEntry -> OpenOrCreateEntry* -> SendRequest*
//
// 7. HEAD. Cached entry, no validation:
//   Start():
//   The same flow as for a GET request (example #2)
//
//   Read():
//   CacheReadData (returns 0)
//
// 8. HEAD. Cached entry, validation (304):
//   The request updates the stored headers.
//   Start(): Same as for a GET request (example #3)
//
//   Read():
//   CacheReadData (returns 0)
//
// 9. HEAD. Cached entry, validation and replace (200):
//   Pass through. The request dooms the old entry, as a HEAD won't be stored by
//   itself.
//   Start():
//   GetBackend* -> InitEntry -> OpenOrCreateEntry* -> AddToEntry* ->
//   CacheReadResponse* -> CacheDispatchValidation ->
//   BeginPartialCacheValidation() -> BeginCacheValidation() -> SendRequest* ->
//   SuccessfulSendRequest -> OverwriteCachedResponse -> FinishHeaders*
//
// 10. HEAD. Sparse entry, partially cached:
//   Serve the request from the cache, as long as it doesn't require
//   revalidation. Ignore missing ranges when deciding to revalidate. If the
//   entry requires revalidation, ignore the whole request and go to full pass
//   through (the result of the HEAD request will NOT update the entry).
//
//   Start(): Basically the same as example 7, as we never create a partial_
//   object for this request.
//
// 11. Prefetch, not-cached entry:
//   The same as example 1. The "unused_since_prefetch" bit is stored as true in
//   UpdateCachedResponse.
//
// 12. Prefetch, cached entry:
//   Like examples 2-4, only CacheWriteUpdatedPrefetchResponse* is inserted
//   between CacheReadResponse* and CacheDispatchValidation if the
//   unused_since_prefetch bit is unset.
//
// 13. Cached entry less than 5 minutes old, unused_since_prefetch is true:
//   Skip validation, similar to example 2.
//   GetBackend* -> InitEntry -> OpenOrCreateEntry* -> AddToEntry* ->
//   CacheReadResponse* -> CacheToggleUnusedSincePrefetch* ->
//   CacheDispatchValidation -> BeginPartialCacheValidation() ->
//   BeginCacheValidation() -> ConnectedCallback* -> SetupEntryForRead() ->
//   FinishHeaders*
//
//   Read():
//   CacheReadData*
//
// 14. Cached entry more than 5 minutes old, unused_since_prefetch is true:
//   Like examples 2-4, only CacheToggleUnusedSincePrefetch* is inserted between
//   CacheReadResponse* and CacheDispatchValidation.
int HttpCache::Transaction::DoLoop(int result) {
  DCHECK_NE(STATE_UNSET, next_state_);
  DCHECK_NE(STATE_NONE, next_state_);
  DCHECK(!in_do_loop_);

  int rv = result;
  State state = next_state_;
  do {
    state = next_state_;
    next_state_ = STATE_UNSET;
    base::AutoReset<bool> scoped_in_do_loop(&in_do_loop_, true);

    switch (state) {
      case STATE_GET_BACKEND:
        DCHECK_EQ(OK, rv);
        rv = DoGetBackend();
        break;
      case STATE_GET_BACKEND_COMPLETE:
        rv = DoGetBackendComplete(rv);
        break;
      case STATE_INIT_ENTRY:
        DCHECK_EQ(OK, rv);
        rv = DoInitEntry();
        break;
      case STATE_OPEN_OR_CREATE_ENTRY:
        DCHECK_EQ(OK, rv);
        rv = DoOpenOrCreateEntry();
        break;
      case STATE_OPEN_OR_CREATE_ENTRY_COMPLETE:
        rv = DoOpenOrCreateEntryComplete(rv);
        break;
      case STATE_DOOM_ENTRY:
        DCHECK_EQ(OK, rv);
        rv = DoDoomEntry();
        break;
      case STATE_DOOM_ENTRY_COMPLETE:
        rv = DoDoomEntryComplete(rv);
        break;
      case STATE_CREATE_ENTRY:
        DCHECK_EQ(OK, rv);
        rv = DoCreateEntry();
        break;
      case STATE_CREATE_ENTRY_COMPLETE:
        rv = DoCreateEntryComplete(rv);
        break;
      case STATE_ADD_TO_ENTRY:
        DCHECK_EQ(OK, rv);
        rv = DoAddToEntry();
        break;
      case STATE_ADD_TO_ENTRY_COMPLETE:
        rv = DoAddToEntryComplete(rv);
        break;
      case STATE_DONE_HEADERS_ADD_TO_ENTRY_COMPLETE:
        rv = DoDoneHeadersAddToEntryComplete(rv);
        break;
      case STATE_CACHE_READ_RESPONSE:
        DCHECK_EQ(OK, rv);
        rv = DoCacheReadResponse();
        break;
      case STATE_CACHE_READ_RESPONSE_COMPLETE:
        rv = DoCacheReadResponseComplete(rv);
        break;
      case STATE_WRITE_UPDATED_PREFETCH_RESPONSE:
        DCHECK_EQ(OK, rv);
        rv = DoCacheWriteUpdatedPrefetchResponse(rv);
        break;
      case STATE_WRITE_UPDATED_PREFETCH_RESPONSE_COMPLETE:
        rv = DoCacheWriteUpdatedPrefetchResponseComplete(rv);
        break;
      case STATE_CACHE_DISPATCH_VALIDATION:
        DCHECK_EQ(OK, rv);
        rv = DoCacheDispatchValidation();
        break;
      case STATE_CACHE_QUERY_DATA:
        DCHECK_EQ(OK, rv);
        rv = DoCacheQueryData();
        break;
      case STATE_CACHE_QUERY_DATA_COMPLETE:
        rv = DoCacheQueryDataComplete(rv);
        break;
      case STATE_START_PARTIAL_CACHE_VALIDATION:
        DCHECK_EQ(OK, rv);
        rv = DoStartPartialCacheValidation();
        break;
      case STATE_COMPLETE_PARTIAL_CACHE_VALIDATION:
        rv = DoCompletePartialCacheValidation(rv);
        break;
      case STATE_CACHE_UPDATE_STALE_WHILE_REVALIDATE_TIMEOUT:
        DCHECK_EQ(OK, rv);
        rv = DoCacheUpdateStaleWhileRevalidateTimeout();
        break;
      case STATE_CACHE_UPDATE_STALE_WHILE_REVALIDATE_TIMEOUT_COMPLETE:
        rv = DoCacheUpdateStaleWhileRevalidateTimeoutComplete(rv);
        break;
      case STATE_CONNECTED_CALLBACK:
        rv = DoConnectedCallback();
        break;
      case STATE_CONNECTED_CALLBACK_COMPLETE:
        rv = DoConnectedCallbackComplete(rv);
        break;
      case STATE_SETUP_ENTRY_FOR_READ:
        DCHECK_EQ(OK, rv);
        rv = DoSetupEntryForRead();
        break;
      case STATE_SEND_REQUEST:
        DCHECK_EQ(OK, rv);
        rv = DoSendRequest();
        break;
      case STATE_SEND_REQUEST_COMPLETE:
        rv = DoSendRequestComplete(rv);
        break;
      case STATE_SUCCESSFUL_SEND_REQUEST:
        DCHECK_EQ(OK, rv);
        rv = DoSuccessfulSendRequest();
        break;
      case STATE_UPDATE_CACHED_RESPONSE:
        DCHECK_EQ(OK, rv);
        rv = DoUpdateCachedResponse();
        break;
      case STATE_CACHE_WRITE_UPDATED_RESPONSE:
        DCHECK_EQ(OK, rv);
        rv = DoCacheWriteUpdatedResponse();
        break;
      case STATE_CACHE_WRITE_UPDATED_RESPONSE_COMPLETE:
        rv = DoCacheWriteUpdatedResponseComplete(rv);
        break;
      case STATE_UPDATE_CACHED_RESPONSE_COMPLETE:
        rv = DoUpdateCachedResponseComplete(rv);
        break;
      case STATE_OVERWRITE_CACHED_RESPONSE:
        DCHECK_EQ(OK, rv);
        rv = DoOverwriteCachedResponse();
        break;
      case STATE_CACHE_WRITE_RESPONSE:
        DCHECK_EQ(OK, rv);
        rv = DoCacheWriteResponse();
        break;
      case STATE_CACHE_WRITE_RESPONSE_COMPLETE:
        rv = DoCacheWriteResponseComplete(rv);
        break;
      case STATE_TRUNCATE_CACHED_DATA:
        DCHECK_EQ(OK, rv);
        rv = DoTruncateCachedData();
        break;
      case STATE_TRUNCATE_CACHED_DATA_COMPLETE:
        rv = DoTruncateCachedDataComplete(rv);
        break;
      case STATE_PARTIAL_HEADERS_RECEIVED:
        DCHECK_EQ(OK, rv);
        rv = DoPartialHeadersReceived();
        break;
      case STATE_HEADERS_PHASE_CANNOT_PROCEED:
        rv = DoHeadersPhaseCannotProceed(rv);
        break;
      case STATE_FINISH_HEADERS:
        rv = DoFinishHeaders(rv);
        break;
      case STATE_FINISH_HEADERS_COMPLETE:
        rv = DoFinishHeadersComplete(rv);
        break;
      case STATE_NETWORK_READ_CACHE_WRITE:
        DCHECK_EQ(OK, rv);
        rv = DoNetworkReadCacheWrite();
        break;
      case STATE_NETWORK_READ_CACHE_WRITE_COMPLETE:
        rv = DoNetworkReadCacheWriteComplete(rv);
        break;
      case STATE_CACHE_READ_DATA:
        DCHECK_EQ(OK, rv);
        rv = DoCacheReadData();
        break;
      case STATE_CACHE_READ_DATA_COMPLETE:
        rv = DoCacheReadDataComplete(rv);
        break;
      case STATE_NETWORK_READ:
        DCHECK_EQ(OK, rv);
        rv = DoNetworkRead();
        break;
      case STATE_NETWORK_READ_COMPLETE:
        rv = DoNetworkReadComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state " << state;
    }
    DCHECK(next_state_ != STATE_UNSET) << "Previous state was " << state;

  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  // Assert Start() state machine's allowed last state in successful cases when
  // caching is happening.
  DCHECK(reading_ || rv != OK || !entry_ ||
         state == STATE_FINISH_HEADERS_COMPLETE);

  if (rv != ERR_IO_PENDING && !callback_.is_null()) {
    read_buf_ = nullptr;  // Release the buffer before invoking the callback.
    std::move(callback_).Run(rv);
  }

  return rv;
}

int HttpCache::Transaction::DoGetBackend() {
  cache_pending_ = true;
  TransitionToState(STATE_GET_BACKEND_COMPLETE);
  net_log_.BeginEvent(NetLogEventType::HTTP_CACHE_GET_BACKEND);
  return cache_->GetBackendForTransaction(this);
}

int HttpCache::Transaction::DoGetBackendComplete(int result) {
  DCHECK(result == OK || result == ERR_FAILED);
  net_log_.EndEventWithNetErrorCode(NetLogEventType::HTTP_CACHE_GET_BACKEND,
                                    result);
  cache_pending_ = false;

  // Reset mode_ that might get set in this function. This is done because this
  // function can be invoked multiple times for a transaction.
  mode_ = NONE;
  const bool should_pass_through = ShouldPassThrough();

  std::optional<std::string> cache_key =
      HttpCache::GenerateCacheKeyForRequest(request_);

  // If no cache key is generated from this request, treat that the same way we
  // do other pass-through cases. This prevents resources whose origin is opaque
  // from being cached. Blink's memory cache should take care of reusing
  // resources within the current page load, but otherwise a resource with an
  // opaque top-frame origin won’t be used again. Also, if the request does not
  // have a top frame origin, bypass the cache otherwise resources from
  // different pages could share a cached entry in such cases.
  if (!should_pass_through && cache_key.has_value()) {
    cache_key_ = *cache_key;

    // Requested cache access mode.
    if (effective_load_flags_ & LOAD_ONLY_FROM_CACHE) {
      if (effective_load_flags_ & LOAD_BYPASS_CACHE) {
        // The client has asked for nonsense.
        TransitionToState(STATE_FINISH_HEADERS);
        return ERR_CACHE_MISS;
      }
      mode_ = READ;
    } else if (effective_load_flags_ & LOAD_BYPASS_CACHE) {
      mode_ = WRITE;
    } else {
      CHECK(!done_headers_create_new_entry_);
      mode_ = READ_WRITE;
    }

    // Downgrade to UPDATE if the request has been externally conditionalized.
    if (external_validation_) {
      if (mode_ & WRITE) {
        // Strip off the READ_DATA bit (and maybe add back a READ_META bit
        // in case READ was off).
        mode_ = UPDATE;
      } else {
        mode_ = NONE;
      }
    }
  }

  // Use PUT, DELETE, and PATCH only to invalidate existing stored entries.
  if ((method_ == "PUT" || method_ == "DELETE" || method_ == "PATCH") &&
      mode_ != READ_WRITE && mode_ != WRITE) {
    mode_ = NONE;
  }

  // Note that if mode_ == UPDATE (which is tied to external_validation_), the
  // transaction behaves the same for GET and HEAD requests at this point: if it
  // was not modified, the entry is updated and a response is not returned from
  // the cache. If we receive 200, it doesn't matter if there was a validation
  // header or not.
  if (method_ == "HEAD" && mode_ == WRITE) {
    mode_ = NONE;
  }

  // If must use cache, then we must fail.  This can happen for back/forward
  // navigations to a page generated via a form post.
  if (!(mode_ & READ) && effective_load_flags_ & LOAD_ONLY_FROM_CACHE) {
    TransitionToState(STATE_FINISH_HEADERS);
    return ERR_CACHE_MISS;
  }

  if (mode_ == NONE) {
    if (partial_) {
      partial_->RestoreHeaders(&mutable_request_->extra_headers);
      partial_.reset();
    }
    TransitionToState(STATE_SEND_REQUEST);
  } else {
    TransitionToState(STATE_INIT_ENTRY);
  }

  // This is only set if we have something to do with the response.
  range_requested_ = (partial_.get() != nullptr);

  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoGetBackendComplete",
                      track_for_state_change_, "mode", mode_,
                      "should_pass_through", should_pass_through);
  return OK;
}

int HttpCache::Transaction::DoInitEntry() {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoInitEntry",
                      track_for_state_change_);
  DCHECK(!new_entry_);

  if (!cache_.get()) {
    TransitionToState(STATE_FINISH_HEADERS);
    return ERR_UNEXPECTED;
  }

  if (mode_ == WRITE) {
    TransitionToState(STATE_DOOM_ENTRY);
    return OK;
  }

  // No-Vary-Search is only useful if we are going to read from the cache. We
  // enable it for externally conditionalized requests as it may be useful if
  // JavaScript is doing its own cache revalidation, and it will provide more
  // consistent behavior.
  if ((mode_ & READ_META) && read_no_vary_search_cache_ &&
      IsNoVarySearchApplicable()) {
    no_vary_search_use_result_ = LookupRequestInNoVarySearchCache();
    if (no_vary_search_use_result_ == NoVarySearchUseResult::kUsed &&
        first_nvs_cache_lookup_end_time_.is_null()) {
      first_nvs_cache_lookup_end_time_ = base::TimeTicks::Now();
    }
  } else if (!first_nvs_cache_lookup_end_time_.is_null() &&
             no_vary_search_use_result_ != NoVarySearchUseResult::kUsed) {
    // A NoVarySearchCache lookup succeeded earlier for this transaction, but
    // then for some reason the result was unusable. Record the time lost as a
    // result. See the histogram "HttpCache.NoVarySearch.UseResult" for
    // information about what went wrong.
    const base::TimeDelta elapsed =
        base::TimeTicks::Now() - first_nvs_cache_lookup_end_time_;

    base::UmaHistogramTimes("HttpCache.NoVarySearch.NotUsableLostTime2",
                            elapsed);
    if (no_vary_search_use_result_ == NoVarySearchUseResult::kNotSuitable) {
      // In this case, we detected that the entry was unusable using in-memory
      // hints, so we should have returned to this point extremely quickly. This
      // histogram verifies that we did.
      base::UmaHistogramCustomMicrosecondsTimes(
          "HttpCache.NoVarySearch.NotUsableLostTime2.NotSuitable", elapsed,
          base::Microseconds(1), base::Seconds(1), 50);
    }
    if ((effective_load_flags_ & LOAD_MAIN_FRAME_DEPRECATED) &&
        IsGoogleHostWithAlpnH3(request_->url.host())) {
      base::UmaHistogramTimes(
          "HttpCache.NoVarySearch.NotUsableLostTime2.GoogleHost.MainFrame",
          elapsed);
    }
    first_nvs_cache_lookup_end_time_ = base::TimeTicks();
  }

  TransitionToState(STATE_OPEN_OR_CREATE_ENTRY);
  return OK;
}

int HttpCache::Transaction::DoOpenOrCreateEntry() {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoOpenOrCreateEntry",
                      track_for_state_change_);
  DCHECK(!new_entry_);
  TransitionToState(STATE_OPEN_OR_CREATE_ENTRY_COMPLETE);
  cache_pending_ = true;
  net_log_.BeginEvent(NetLogEventType::HTTP_CACHE_OPEN_OR_CREATE_ENTRY);
  first_cache_access_since_ = TimeTicks::Now();
  const bool has_opened_or_created_entry = has_opened_or_created_entry_;
  has_opened_or_created_entry_ = true;
  record_entry_open_or_creation_time_ = false;

  // See if we already have something working with this cache key.
  new_entry_ = cache_->GetActiveEntry(cache_key_);
  if (new_entry_) {
    return OK;
  }

  // See if we could potentially doom the entry based on hints the backend keeps
  // in memory.
  // Currently only SimpleCache utilizes in memory hints. If an entry is found
  // unsuitable, and thus Doomed, SimpleCache can also optimize the
  // OpenOrCreateEntry() call to reduce the overhead of trying to open an entry
  // we know is doomed.
  uint8_t in_memory_info =
      cache_->GetCurrentBackend()->GetEntryInMemoryData(cache_key_);
  bool entry_not_suitable = false;
  if (MaybeRejectBasedOnEntryInMemoryData(in_memory_info)) {
    // If the URL was rewritten by the NoVarySearchCache we may want to use it
    // again. The transaction will be restarted with the unmodified URL, so we
    // don't need to delete the entry for correctness.
    if (!(features::kHttpCacheNoVarySearchKeepNotSuitable.Get() &&
          IsUsingURLFromNoVarySearchCache())) {
      cache_->GetCurrentBackend()->DoomEntry(cache_key_, priority_,
                                             base::DoNothing());
    }
    entry_not_suitable = true;
    // Documents the case this applies in
    DCHECK_EQ(mode_, READ_WRITE);
    // Record this as CantConditionalize, but otherwise proceed as we would
    // below --- as we've already dropped the old entry.
    couldnt_conditionalize_request_ = true;
    UpdateCacheEntryStatus(CacheEntryStatus::ENTRY_CANT_CONDITIONALIZE);
  }

  if (!has_opened_or_created_entry) {
    record_entry_open_or_creation_time_ = true;
  }

  if (base::FeatureList::IsEnabled(features::kAvoidEntryCreationForNoStore)) {
    // TODO(http://crbug.com/331123686): There is no reason to make partial
    // requests exempt but for now some tests fail if we don't. Once the bug
    // is fixed and understood it will be possible to remove this line.
    if (!partial_) {
      if (cache_->DidKeyLeadToNoStoreResponse(cache_key_)) {
        // The request is probably not suitable for caching and is there is
        // nothing to open.
        return ERR_CACHE_ENTRY_NOT_SUITABLE;
      }
    }
  }

  // mode_ can be anything but NONE or WRITE at this point (READ, UPDATE, or
  // READ_WRITE).
  // READ, UPDATE, certain READ_WRITEs, and some methods shouldn't create, so
  // try only opening.
  CHECK_NE(mode_, NONE);
  CHECK_NE(mode_, WRITE);
  if (mode_ != READ_WRITE || ShouldOpenOnlyMethods()) {
    if (entry_not_suitable) {
      // The entry isn't suitable and we can't create a new one.
      return ERR_CACHE_ENTRY_NOT_SUITABLE;
    }

    return cache_->OpenEntry(cache_key_, &new_entry_, this);
  }

  if (IsUsingURLFromNoVarySearchCache()) {
    // We should never create a new entry with the original URL.
    if (entry_not_suitable) {
      return ERR_CACHE_ENTRY_NOT_SUITABLE;
    }

    return cache_->OpenEntry(cache_key_, &new_entry_, this);
  }

  return cache_->OpenOrCreateEntry(cache_key_, &new_entry_, this);
}

int HttpCache::Transaction::DoOpenOrCreateEntryComplete(int result) {
  TRACE_EVENT_INSTANT(
      TRACE_DISABLED_BY_DEFAULT("net"), "DoOpenOrCreateEntryComplete",
      track_for_state_change_, "result",
      (result == OK ? (new_entry_->opened() ? "opened" : "created")
                    : "failed"));

  const bool record_uma =
      record_entry_open_or_creation_time_ && cache_ &&
      cache_->GetCurrentBackend() &&
      cache_->GetCurrentBackend()->GetCacheType() != MEMORY_CACHE;
  record_entry_open_or_creation_time_ = false;

  // It is important that we go to STATE_ADD_TO_ENTRY whenever the result is
  // OK, otherwise the cache will end up with an active entry without any
  // transaction attached.
  net_log_.EndEvent(NetLogEventType::HTTP_CACHE_OPEN_OR_CREATE_ENTRY, [&] {
    base::Value::Dict params;
    if (result == OK) {
      params.Set("result", new_entry_->opened() ? "opened" : "created");
    } else {
      params.Set("net_error", result);
    }
    return params;
  });

  cache_pending_ = false;

  if (result == OK) {
    if (new_entry_->opened()) {
      if (record_uma) {
        base::UmaHistogramTimes(
            "HttpCache.OpenDiskEntry",
            base::TimeTicks::Now() - first_cache_access_since_);
      }
    } else {
      if (record_uma) {
        base::UmaHistogramTimes(
            "HttpCache.CreateDiskEntry",
            base::TimeTicks::Now() - first_cache_access_since_);
      }

      CHECK(!IsUsingURLFromNoVarySearchCache());

      // Entry was created so mode changes to WRITE.
      mode_ = WRITE;
    }

    TransitionToState(STATE_ADD_TO_ENTRY);
    return OK;
  }

  if (result == ERR_CACHE_RACE) {
    TransitionToState(STATE_HEADERS_PHASE_CANNOT_PROCEED);
    return OK;
  }

  // This handles the case where opening the disk cache entry failed, or it was
  // found to be unusable due to in-memory flags.
  if (IsUsingURLFromNoVarySearchCache()) {
    if (result == ERR_CACHE_ENTRY_NOT_SUITABLE) {
      return RestartWithoutNoVarySearchCache(
          features::kHttpCacheNoVarySearchKeepNotSuitable.Get()
              ? RestartCacheEntryAction::kDontErase
              : RestartCacheEntryAction::kErase,
          NoVarySearchUseResult::kNotSuitable);
    }

    return RestartWithoutNoVarySearchCache(RestartCacheEntryAction::kErase,
                                           NoVarySearchUseResult::kNotOpenable);
  }

  if (ShouldOpenOnlyMethods() || result == ERR_CACHE_ENTRY_NOT_SUITABLE) {
    // Bypassing the cache.
    mode_ = NONE;
    TransitionToState(STATE_SEND_REQUEST);
    return OK;
  }

  // Since the operation failed, what we do next depends on the mode_ which
  // can be the following: READ, READ_WRITE, or UPDATE. Note: mode_ cannot be
  // WRITE or NONE at this point as DoInitEntry() handled those cases.

  switch (mode_) {
    case READ:
      // The entry does not exist, and we are not permitted to create a new
      // entry, so we must fail.
      TransitionToState(STATE_FINISH_HEADERS);
      return ERR_CACHE_MISS;
    case READ_WRITE:
      // Unable to open or create; set the mode to NONE in order to bypass the
      // cache entry and read from the network directly.
      mode_ = NONE;
      if (partial_) {
        partial_->RestoreHeaders(&mutable_request_->extra_headers);
      }
      TransitionToState(STATE_SEND_REQUEST);
      break;
    case UPDATE:
      // There is no cache entry to update; proceed without caching.
      DCHECK(!partial_);
      mode_ = NONE;
      TransitionToState(STATE_SEND_REQUEST);
      break;
    default:
      NOTREACHED();
  }

  return OK;
}

int HttpCache::Transaction::DoDoomEntry() {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoDoomEntry",
                      track_for_state_change_);
  TransitionToState(STATE_DOOM_ENTRY_COMPLETE);
  cache_pending_ = true;
  if (first_cache_access_since_.is_null()) {
    first_cache_access_since_ = TimeTicks::Now();
  }
  net_log_.BeginEvent(NetLogEventType::HTTP_CACHE_DOOM_ENTRY);
  return cache_->DoomEntry(cache_key_, this);
}

int HttpCache::Transaction::DoDoomEntryComplete(int result) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoDoomEntryComplete",
                      track_for_state_change_, "result", result);
  net_log_.EndEventWithNetErrorCode(NetLogEventType::HTTP_CACHE_DOOM_ENTRY,
                                    result);
  cache_pending_ = false;
  TransitionToState(result == ERR_CACHE_RACE
                        ? STATE_HEADERS_PHASE_CANNOT_PROCEED
                        : STATE_CREATE_ENTRY);
  return OK;
}

int HttpCache::Transaction::DoCreateEntry() {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoCreateEntry",
                      track_for_state_change_);
  DCHECK(!new_entry_);
  TransitionToState(STATE_CREATE_ENTRY_COMPLETE);
  cache_pending_ = true;
  net_log_.BeginEvent(NetLogEventType::HTTP_CACHE_CREATE_ENTRY);
  return cache_->CreateEntry(cache_key_, &new_entry_, this);
}

int HttpCache::Transaction::DoCreateEntryComplete(int result) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoCreateEntryComplete",
                      track_for_state_change_, "result", result);
  // It is important that we go to STATE_ADD_TO_ENTRY whenever the result is
  // OK, otherwise the cache will end up with an active entry without any
  // transaction attached.
  net_log_.EndEventWithNetErrorCode(NetLogEventType::HTTP_CACHE_CREATE_ENTRY,
                                    result);
  cache_pending_ = false;
  switch (result) {
    case OK:
      TransitionToState(STATE_ADD_TO_ENTRY);
      break;

    case ERR_CACHE_RACE:
      TransitionToState(STATE_HEADERS_PHASE_CANNOT_PROCEED);
      break;

    default:
      DLOG(WARNING) << "Unable to create cache entry";

      // Set the mode to NONE in order to bypass the cache entry and read from
      // the network directly.
      mode_ = NONE;
      if (!done_headers_create_new_entry_) {
        if (partial_) {
          partial_->RestoreHeaders(&mutable_request_->extra_headers);
        }
        CHECK(!IsUsingURLFromNoVarySearchCache());
        TransitionToState(STATE_SEND_REQUEST);
        return OK;
      }
      // The headers have already been received as a result of validation,
      // triggering the doom of the old entry.  So no network request needs to
      // be sent. Note that since mode_ is NONE, the response won't be written
      // to cache. Transition to STATE_CACHE_WRITE_RESPONSE as that's the state
      // the transaction left off on when it tried to create the new entry.
      done_headers_create_new_entry_ = false;
      TransitionToState(STATE_CACHE_WRITE_RESPONSE);
  }
  return OK;
}

int HttpCache::Transaction::DoAddToEntry() {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoAddToEntry",
                      track_for_state_change_);
  DCHECK(new_entry_);
  cache_pending_ = true;
  net_log_.BeginEvent(NetLogEventType::HTTP_CACHE_ADD_TO_ENTRY);
  DCHECK(entry_lock_waiting_since_.is_null());

  // By this point whether the entry was created or opened is no longer relevant
  // for this transaction. However there may be queued transactions that want to
  // use this entry and from their perspective the entry was opened, so change
  // the flag to reflect that.
  new_entry_->set_opened(true);

  int rv = cache_->AddTransactionToEntry(new_entry_, this);
  CHECK_EQ(rv, ERR_IO_PENDING);

  // If headers phase is already done then we are here because of validation not
  // matching and creating a new entry. This transaction should be the
  // first transaction of that new entry and thus it will not have cache lock
  // delays, thus returning early from here.
  if (done_headers_create_new_entry_) {
    CHECK_EQ(mode_, WRITE);
    TransitionToState(STATE_DONE_HEADERS_ADD_TO_ENTRY_COMPLETE);
    return rv;
  }

  TransitionToState(STATE_ADD_TO_ENTRY_COMPLETE);

  // For a very-select case of creating a new non-range request entry, run the
  // AddTransactionToEntry in parallel with sending the network request to
  // hide the latency. This will run until the next ERR_IO_PENDING (or
  // failure).
  if (!partial_ && mode_ == WRITE) {
    CHECK(!waiting_for_cache_io_);
    waiting_for_cache_io_ = true;
    rv = OK;
  }

  entry_lock_waiting_since_ = TimeTicks::Now();
  AddCacheLockTimeoutHandler(new_entry_.get());
  return rv;
}

void HttpCache::Transaction::AddCacheLockTimeoutHandler(ActiveEntry* entry) {
  CHECK(next_state_ == STATE_ADD_TO_ENTRY_COMPLETE ||
        next_state_ == STATE_FINISH_HEADERS_COMPLETE);
  if ((bypass_lock_for_test_ && next_state_ == STATE_ADD_TO_ENTRY_COMPLETE) ||
      (bypass_lock_after_headers_for_test_ &&
       next_state_ == STATE_FINISH_HEADERS_COMPLETE)) {
    TaskRunner(priority_)->PostTask(
        FROM_HERE,
        base::BindOnce(&HttpCache::Transaction::OnCacheLockTimeout,
                       weak_factory_.GetWeakPtr(), entry_lock_waiting_since_));
  } else {
    int timeout_milliseconds = 20 * 1000;
    if (partial_ && entry->HasWriters() && !entry->writers()->IsEmpty() &&
        entry->writers()->IsExclusive()) {
      // Even though entry_->writers takes care of allowing multiple writers to
      // simultaneously govern reading from the network and writing to the cache
      // for full requests, partial requests are still blocked by the
      // reader/writer lock.
      // Bypassing the cache after 25 ms of waiting for the cache lock
      // eliminates a long running issue, http://crbug.com/31014, where
      // two of the same media resources could not be played back simultaneously
      // due to one locking the cache entry until the entire video was
      // downloaded.
      // Bypassing the cache is not ideal, as we are now ignoring the cache
      // entirely for all range requests to a resource beyond the first. This
      // is however a much more succinct solution than the alternatives, which
      // would require somewhat significant changes to the http caching logic.
      //
      // Allow some timeout slack for the entry addition to complete in case
      // the writer lock is imminently released; we want to avoid skipping
      // the cache if at all possible. See http://crbug.com/408765
      timeout_milliseconds = 25;
    }
    TaskRunner(priority_)->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&HttpCache::Transaction::OnCacheLockTimeout,
                       weak_factory_.GetWeakPtr(), entry_lock_waiting_since_),
        base::Milliseconds(timeout_milliseconds));
  }
}

int HttpCache::Transaction::DoAddToEntryComplete(int result) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoAddToEntryComplete",
                      track_for_state_change_, "result", result);
  net_log_.EndEventWithNetErrorCode(NetLogEventType::HTTP_CACHE_ADD_TO_ENTRY,
                                    result);
  if (cache_ && cache_->GetCurrentBackend() &&
      cache_->GetCurrentBackend()->GetCacheType() != MEMORY_CACHE) {
    const base::TimeDelta entry_lock_wait =
        TimeTicks::Now() - entry_lock_waiting_since_;
    base::UmaHistogramTimes("HttpCache.AddTransactionToEntry", entry_lock_wait);
  }

  DCHECK(new_entry_);

  if (!waiting_for_cache_io_) {
    entry_lock_waiting_since_ = TimeTicks();
    cache_pending_ = false;

    if (result == OK) {
      entry_ = std::move(new_entry_);
    }

    // If there is a failure, the cache should have taken care of new_entry_.
    new_entry_.reset();
  }

  if (result == ERR_CACHE_RACE) {
    TransitionToState(STATE_HEADERS_PHASE_CANNOT_PROCEED);
    return OK;
  }

  if (result == ERR_CACHE_LOCK_TIMEOUT) {
    if (mode_ == READ) {
      TransitionToState(STATE_FINISH_HEADERS);
      return ERR_CACHE_MISS;
    }

    if (IsUsingURLFromNoVarySearchCache()) {
      // The entry is probably fine, we just couldn't get a lock on it this
      // time. The restarted request is permitted to attempt to get a cache lock
      // for the original URL, and on successful completion may later replace
      // the entry in the NoVarySearchCache.
      return RestartWithoutNoVarySearchCache(
          RestartCacheEntryAction::kDontErase,
          NoVarySearchUseResult::kCacheLockTimeout);
    }

    // The cache is busy, bypass it for this transaction.
    mode_ = NONE;
    TransitionToState(STATE_SEND_REQUEST);
    if (partial_) {
      partial_->RestoreHeaders(&mutable_request_->extra_headers);
      partial_.reset();
    }
    return OK;
  }

  if (result != OK) {
    NOTREACHED();
  }

  if (mode_ == WRITE) {
    if (partial_) {
      partial_->RestoreHeaders(&mutable_request_->extra_headers);
    }
    TransitionToState(STATE_SEND_REQUEST);
  } else {
    // We have to read the headers from the cached entry.
    DCHECK(mode_ & READ_META);
    TransitionToState(STATE_CACHE_READ_RESPONSE);
  }
  return OK;
}

int HttpCache::Transaction::DoDoneHeadersAddToEntryComplete(int result) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoDoneHeadersAddToEntryComplete",
                      track_for_state_change_, "result", result);
  // This transaction's response headers did not match its ActiveEntry so it
  // created a new ActiveEntry (new_entry_) to write to (and doomed the old
  // one). Now that the new entry has been created, start writing the response.

  DCHECK_EQ(result, OK);
  DCHECK_EQ(mode_, WRITE);
  DCHECK(new_entry_);
  DCHECK(response_.headers);

  cache_pending_ = false;
  done_headers_create_new_entry_ = false;

  // It is unclear exactly how this state is reached with an ERR_CACHE_RACE, but
  // this check appears to fix a rare crash. See crbug.com/959194.
  if (result == ERR_CACHE_RACE) {
    TransitionToState(STATE_HEADERS_PHASE_CANNOT_PROCEED);
    return OK;
  }

  entry_ = std::move(new_entry_);
  DCHECK_NE(response_.headers->response_code(), HTTP_NOT_MODIFIED);
  DCHECK(entry_->CanTransactionWriteResponseHeaders(this, partial_ != nullptr,
                                                    false));
  TransitionToState(STATE_CACHE_WRITE_RESPONSE);
  return OK;
}

int HttpCache::Transaction::DoCacheReadResponse() {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoCacheReadResponse",
                      track_for_state_change_);
  DCHECK(entry_);
  TransitionToState(STATE_CACHE_READ_RESPONSE_COMPLETE);

  io_buf_len_ = entry_->GetEntry()->GetDataSize(kResponseInfoIndex);
  read_buf_ = base::MakeRefCounted<IOBufferWithSize>(io_buf_len_);

  net_log_.BeginEvent(NetLogEventType::HTTP_CACHE_READ_INFO);
  BeginDiskCacheAccessTimeCount();
  return entry_->GetEntry()->ReadData(kResponseInfoIndex, 0, read_buf_.get(),
                                      io_buf_len_, io_callback_);
}

int HttpCache::Transaction::DoCacheReadResponseComplete(int result) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoCacheReadResponseComplete", track_for_state_change_,
                      "result", result, "io_buf_len", read_buf_->size());
  net_log_.EndEventWithNetErrorCode(NetLogEventType::HTTP_CACHE_READ_INFO,
                                    result);
  EndDiskCacheAccessTimeCount(DiskCacheAccessType::kRead);

  // Record the time immediately before the cached response is parsed.
  read_headers_since_ = TimeTicks::Now();

  if (result != read_buf_->size() ||
      !HttpCache::ParseResponseInfo(read_buf_->span(), &response_,
                                    &truncated_)) {
    return OnCacheReadError(result, true);
  }

  // If the read response matches the clearing filter of FPS, doom the entry
  // and restart transaction.
  if (ShouldByPassCacheForFirstPartySets(initial_request_->fps_cache_filter,
                                         response_.browser_run_id)) {
    result = ERR_CACHE_ENTRY_NOT_SUITABLE;
    return OnCacheReadError(result, true);
  }

  // TODO(https://crbug.com/40516423): Only get data size if there is no other
  // transaction currently writing the response body due to the data race
  // mentioned in the associated bug.
  if (!entry_->IsWritingInProgress()) {
    int current_size = entry_->GetEntry()->GetDataSize(kResponseContentIndex);
    std::optional<base::ByteCount> content_length =
        response_.headers->GetContentLength();

    // Some resources may have slipped in as truncated when they're not.
    if (content_length && content_length->InBytes() == current_size) {
      truncated_ = false;
    }

    // The state machine's handling of StopCaching unfortunately doesn't deal
    // well with resources that are larger than 2GB when there is a truncated or
    // sparse cache entry. While the state machine is reworked to resolve this,
    // the following logic is put in place to defer such requests to the
    // network. The cache should not be storing multi gigabyte resources. See
    // https://crbug.com/40598279.
    if ((truncated_ ||
         response_.headers->response_code() == HTTP_PARTIAL_CONTENT) &&
        !range_requested_ && content_length &&
        content_length->InBytes() > std::numeric_limits<int32_t>::max()) {
      DCHECK(!partial_);

      // Doom the entry so that no other transaction gets added to this entry
      // and avoid a race of not being able to check this condition because
      // writing is in progress.
      DoneWithEntry(false);
      TransitionToState(STATE_SEND_REQUEST);
      return OK;
    }
  }

  if (response_.restricted_prefetch &&
      !(request_->load_flags &
        LOAD_CAN_USE_RESTRICTED_PREFETCH_FOR_MAIN_FRAME)) {
    TransitionToState(STATE_SEND_REQUEST);
    return OK;
  }

  // When a restricted prefetch is reused, we lift its reuse restriction.
  bool restricted_prefetch_reuse =
      response_.restricted_prefetch &&
      request_->load_flags & LOAD_CAN_USE_RESTRICTED_PREFETCH_FOR_MAIN_FRAME;
  DCHECK(!restricted_prefetch_reuse || response_.unused_since_prefetch);

  if (response_.unused_since_prefetch !=
      !!(request_->load_flags & LOAD_PREFETCH)) {
    // Either this is the first use of an entry since it was prefetched XOR
    // this is a prefetch. The value of response.unused_since_prefetch is
    // valid for this transaction but the bit needs to be flipped in storage.
    DCHECK(!updated_prefetch_response_);
    updated_prefetch_response_ = std::make_unique<HttpResponseInfo>(response_);
    updated_prefetch_response_->unused_since_prefetch =
        !response_.unused_since_prefetch;
    if (response_.restricted_prefetch &&
        request_->load_flags &
            LOAD_CAN_USE_RESTRICTED_PREFETCH_FOR_MAIN_FRAME) {
      updated_prefetch_response_->restricted_prefetch = false;
    }

    TransitionToState(STATE_WRITE_UPDATED_PREFETCH_RESPONSE);
    return OK;
  }

  TransitionToState(STATE_CACHE_DISPATCH_VALIDATION);
  return OK;
}

int HttpCache::Transaction::DoCacheWriteUpdatedPrefetchResponse(int result) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoCacheWriteUpdatedPrefetchResponse",
                      track_for_state_change_, "result", result);
  DCHECK(updated_prefetch_response_);
  // TODO(jkarlin): If DoUpdateCachedResponse is also called for this
  // transaction then metadata will be written to cache twice. If prefetching
  // becomes more common, consider combining the writes.
  TransitionToState(STATE_WRITE_UPDATED_PREFETCH_RESPONSE_COMPLETE);
  return WriteResponseInfoToEntry(*updated_prefetch_response_.get(),
                                  truncated_);
}

int HttpCache::Transaction::DoCacheWriteUpdatedPrefetchResponseComplete(
    int result) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoCacheWriteUpdatedPrefetchResponseComplete",
                      track_for_state_change_, "result", result);
  updated_prefetch_response_.reset();
  TransitionToState(STATE_CACHE_DISPATCH_VALIDATION);
  return OnWriteResponseInfoToEntryComplete(result);
}

int HttpCache::Transaction::DoCacheDispatchValidation() {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoCacheDispatchValidation", track_for_state_change_);
  if (!entry_) {
    // Entry got destroyed when twiddling unused-since-prefetch bit.
    TransitionToState(STATE_HEADERS_PHASE_CANNOT_PROCEED);
    return OK;
  }

  // We now have access to the cache entry.
  //
  //  o if we are a reader for the transaction, then we can start reading the
  //    cache entry.
  //
  //  o if we can read or write, then we should check if the cache entry needs
  //    to be validated and then issue a network request if needed or just read
  //    from the cache if the cache entry is already valid.
  //
  //  o if we are set to UPDATE, then we are handling an externally
  //    conditionalized request (if-modified-since / if-none-match). We check
  //    if the request headers define a validation request.
  //
  int result = ERR_FAILED;
  switch (mode_) {
    case READ:
      UpdateCacheEntryStatus(CacheEntryStatus::ENTRY_USED);
      result = BeginCacheRead();
      break;
    case READ_WRITE:
      result = BeginPartialCacheValidation();
      break;
    case UPDATE:
      result = BeginExternallyConditionalizedRequest();
      break;
    case WRITE:
    default:
      NOTREACHED();
  }
  return result;
}

int HttpCache::Transaction::DoCacheQueryData() {
  TransitionToState(STATE_CACHE_QUERY_DATA_COMPLETE);
  return entry_->GetEntry()->ReadyForSparseIO(io_callback_);
}

int HttpCache::Transaction::DoCacheQueryDataComplete(int result) {
  DCHECK_EQ(OK, result);
  if (!cache_.get()) {
    TransitionToState(STATE_FINISH_HEADERS);
    return ERR_UNEXPECTED;
  }

  return ValidateEntryHeadersAndContinue();
}

// We may end up here multiple times for a given request.
int HttpCache::Transaction::DoStartPartialCacheValidation() {
  if (mode_ == NONE) {
    TransitionToState(STATE_FINISH_HEADERS);
    return OK;
  }

  TransitionToState(STATE_COMPLETE_PARTIAL_CACHE_VALIDATION);
  return partial_->ShouldValidateCache(entry_->GetEntry(), io_callback_);
}

int HttpCache::Transaction::DoCompletePartialCacheValidation(int result) {
  if (!result && reading_) {
    // This is the end of the request.
    DoneWithEntry(true);
    TransitionToState(STATE_FINISH_HEADERS);
    return result;
  }

  if (result < 0) {
    TransitionToState(STATE_FINISH_HEADERS);
    return result;
  }

  partial_->PrepareCacheValidation(entry_->GetEntry(),
                                   &mutable_request_->extra_headers);

  if (reading_ && partial_->IsCurrentRangeCached()) {
    // We're about to read a range of bytes from the cache. Signal it to the
    // consumer through the "connected" callback.
    TransitionToState(STATE_CONNECTED_CALLBACK);
    return OK;
  }

  return BeginCacheValidation();
}

int HttpCache::Transaction::DoCacheUpdateStaleWhileRevalidateTimeout() {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoCacheUpdateStaleWhileRevalidateTimeout",
                      track_for_state_change_);
  response_.stale_revalidate_timeout =
      cache_->clock_->Now() + kStaleRevalidateTimeout;
  TransitionToState(STATE_CACHE_UPDATE_STALE_WHILE_REVALIDATE_TIMEOUT_COMPLETE);

  // We shouldn't be using stale truncated entries; if we did, the false below
  // would be wrong.
  CHECK(!truncated_);
  return WriteResponseInfoToEntry(response_, false);
}

int HttpCache::Transaction::DoCacheUpdateStaleWhileRevalidateTimeoutComplete(
    int result) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoCacheUpdateStaleWhileRevalidateTimeoutComplete",
                      track_for_state_change_, "result", result);
  DCHECK(!reading_);
  TransitionToState(STATE_CONNECTED_CALLBACK);
  return OnWriteResponseInfoToEntryComplete(result);
}

int HttpCache::Transaction::DoSendRequest() {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoSendRequest",
                      track_for_state_change_);
  DCHECK(mode_ & WRITE || mode_ == NONE);
  DCHECK(!network_trans_.get());

  send_request_since_ = TimeTicks::Now();

  // Create a network transaction.
  network_trans_ = cache_->network_layer_->CreateTransaction(priority_);
  CHECK(network_trans_);

  network_trans_->SetConnectedCallback(connected_callback_);
  network_trans_->SetRequestHeadersCallback(request_headers_callback_);
  network_trans_->SetEarlyResponseHeadersCallback(
      early_response_headers_callback_);
  network_trans_->SetResponseHeadersCallback(response_headers_callback_);
  if (is_shared_dictionary_read_allowed_callback_) {
    network_trans_->SetIsSharedDictionaryReadAllowedCallback(
        is_shared_dictionary_read_allowed_callback_);
  }

  // Old load timing information, if any, is now obsolete.
  network_transaction_info_.old_network_trans_load_timing.reset();
  network_transaction_info_.old_remote_endpoint = IPEndPoint();

  if (websocket_handshake_stream_base_create_helper_) {
    network_trans_->SetWebSocketHandshakeStreamCreateHelper(
        websocket_handshake_stream_base_create_helper_);
  }

  if (IsUsingURLFromNoVarySearchCache()) {
    // If we are using the NoVarySearchCache, double-check that the network
    // request we are about to send is conditionalized and not a range request.
    CHECK(mode_ & READ_META);
    CHECK(!couldnt_conditionalize_request_);
    CHECK(!partial_);
  }

  TransitionToState(STATE_SEND_REQUEST_COMPLETE);
  int rv = network_trans_->Start(request_, io_callback_, net_log_);
  if (rv != ERR_IO_PENDING && waiting_for_cache_io_) {
    // queue the state transition until the HttpCache transaction completes
    DCHECK(!pending_io_result_);
    pending_io_result_ = rv;
    rv = ERR_IO_PENDING;
  }
  return rv;
}

int HttpCache::Transaction::DoSendRequestComplete(int result) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoSendRequestComplete",
                      track_for_state_change_, "result", result, "elapsed",
                      base::TimeTicks::Now() - send_request_since_);
  if (!cache_.get()) {
    TransitionToState(STATE_FINISH_HEADERS);
    return ERR_UNEXPECTED;
  }

  // If we tried to conditionalize the request and failed, we know
  // we won't be reading from the cache after this point.
  if (couldnt_conditionalize_request_) {
    mode_ = WRITE;
  }

  if (result == OK) {
    TransitionToState(STATE_SUCCESSFUL_SEND_REQUEST);
    return OK;
  }

  const HttpResponseInfo* response = network_trans_->GetResponseInfo();
  response_.network_accessed = response->network_accessed;
  response_.proxy_chain = response->proxy_chain;
  response_.restricted_prefetch = response->restricted_prefetch;
  response_.resolve_error_info = response->resolve_error_info;

  // Do not record requests that have network errors or restarts.
  UpdateCacheEntryStatusToOther(OtherStatusReason::kNetworkError);
  if (IsCertificateError(result)) {
    // If we get a certificate error, then there is a certificate in ssl_info,
    // so GetResponseInfo() should never return NULL here.
    DCHECK(response);
    response_.ssl_info = response->ssl_info;
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    DCHECK(response);
    response_.cert_request_info = response->cert_request_info;
  } else if (result == ERR_INCONSISTENT_IP_ADDRESS_SPACE) {
    DoomInconsistentEntry();
  } else if (response_.was_cached) {
    DoneWithEntry(/*entry_is_complete=*/true);
  }

  TransitionToState(STATE_FINISH_HEADERS);
  return result;
}

// We received the response headers and there is no error.
int HttpCache::Transaction::DoSuccessfulSendRequest() {
  DCHECK(!new_response_);
  const HttpResponseInfo* new_response = network_trans_->GetResponseInfo();
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoSuccessfulSendRequest", track_for_state_change_,
                      "response_code", new_response->headers->response_code());

  if (new_response->headers->response_code() == HTTP_UNAUTHORIZED ||
      new_response->headers->response_code() ==
          HTTP_PROXY_AUTHENTICATION_REQUIRED) {
    SetAuthResponse(*new_response);
    if (!reading_) {
      TransitionToState(STATE_FINISH_HEADERS);
      return OK;
    }

    // We initiated a second request the caller doesn't know about. We should be
    // able to authenticate this request because we should have authenticated
    // this URL moments ago.
    if (IsReadyToRestartForAuth()) {
      TransitionToState(STATE_SEND_REQUEST_COMPLETE);
      // In theory we should check to see if there are new cookies, but there
      // is no way to do that from here.
      return network_trans_->RestartWithAuth(AuthCredentials(), io_callback_);
    }

    // We have to perform cleanup at this point so that at least the next
    // request can succeed.  We do not retry at this point, because data
    // has been read and we have no way to gather credentials.  We would
    // fail again, and potentially loop.  This can happen if the credentials
    // expire while chrome is suspended.
    if (entry_) {
      DoomPartialEntry(false);
    }
    mode_ = NONE;
    partial_.reset();
    ResetNetworkTransaction();
    TransitionToState(STATE_FINISH_HEADERS);
    return ERR_CACHE_AUTH_FAILURE_AFTER_READ;
  }

  new_response_ = new_response;
  if (!ValidatePartialResponse() && !auth_response_.headers.get()) {
    // Something went wrong with this request and we have to restart it.
    // If we have an authentication response, we are exposed to weird things
    // happening if the user cancels the authentication before we receive
    // the new response.
    net_log_.AddEvent(NetLogEventType::HTTP_CACHE_RE_SEND_PARTIAL_REQUEST);
    UpdateCacheEntryStatusToOther(OtherStatusReason::kResponseValidation);
    SetResponse(HttpResponseInfo());
    ResetNetworkTransaction();
    new_response_ = nullptr;
    TransitionToState(STATE_SEND_REQUEST);
    return OK;
  }

  if (handling_206_ && mode_ == READ_WRITE && !truncated_ && !is_sparse_) {
    // We have stored the full entry, but it changed and the server is
    // sending a range. We have to delete the old entry.
    UpdateCacheEntryStatusToOther(OtherStatusReason::kDeleteFullEntry);
    DoneWithEntry(false);
  }

  if (mode_ == WRITE &&
      cache_entry_status_ != CacheEntryStatus::ENTRY_CANT_CONDITIONALIZE) {
    UpdateCacheEntryStatus(CacheEntryStatus::ENTRY_NOT_IN_CACHE);
  }

  // Invalidate any cached GET with a successful PUT, DELETE, or PATCH.
  if (mode_ == WRITE &&
      (method_ == "PUT" || method_ == "DELETE" || method_ == "PATCH")) {
    if (NonErrorResponse(new_response_->headers->response_code()) &&
        (entry_ && !entry_->IsDoomed())) {
      int ret = cache_->DoomEntry(cache_key_, nullptr);
      DCHECK_EQ(OK, ret);
    }
    // Do not invalidate the entry if the request failed.
    DoneWithEntry(true);
  }

  // Invalidate any cached GET with a successful POST. If the network isolation
  // key isn't populated with the split cache active, there will be nothing to
  // invalidate in the cache.
  if (!(effective_load_flags_ & LOAD_DISABLE_CACHE) && method_ == "POST" &&
      NonErrorResponse(new_response_->headers->response_code()) &&
      (!HttpCache::IsSplitCacheEnabled() ||
       request_->network_isolation_key.IsFullyPopulated())) {
    cache_->DoomMainEntryForUrl(request_->url, request_->network_isolation_key,
                                request_->is_subframe_document_resource,
                                request_->is_main_frame_navigation,
                                request_->initiator);
  }

  if (new_response_->headers->response_code() ==
          HTTP_REQUESTED_RANGE_NOT_SATISFIABLE &&
      (method_ == "GET" || method_ == "POST")) {
    // If there is an active entry it may be destroyed with this transaction.
    SetResponse(*new_response_);
    TransitionToState(STATE_FINISH_HEADERS);
    return OK;
  }

  // Are we expecting a response to a conditional query?
  if (mode_ == READ_WRITE || mode_ == UPDATE) {
    if (new_response->headers->response_code() == HTTP_NOT_MODIFIED ||
        handling_206_) {
      UpdateCacheEntryStatus(CacheEntryStatus::ENTRY_VALIDATED);
      TransitionToState(STATE_UPDATE_CACHED_RESPONSE);
      return OK;
    }
    UpdateCacheEntryStatus(CacheEntryStatus::ENTRY_UPDATED);
    mode_ = WRITE;
  }

  TransitionToState(STATE_OVERWRITE_CACHED_RESPONSE);
  return OK;
}

// We received 304 or 206 and we want to update the cached response headers.
int HttpCache::Transaction::DoUpdateCachedResponse() {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoUpdateCachedResponse", track_for_state_change_);
  int rv = OK;
  // Update the cached response based on the headers and properties of
  // new_response_.
  response_.headers->Update(*new_response_->headers.get());
  response_.stale_revalidate_timeout = base::Time();
  response_.response_time = new_response_->response_time;
  if (new_response_->headers->response_code() != net::HTTP_NOT_MODIFIED) {
    response_.original_response_time = new_response_->response_time;
  }
  response_.request_time = new_response_->request_time;
  response_.network_accessed = new_response_->network_accessed;
  response_.unused_since_prefetch = new_response_->unused_since_prefetch;
  response_.restricted_prefetch = new_response_->restricted_prefetch;
  response_.ssl_info = new_response_->ssl_info;
  response_.dns_aliases = new_response_->dns_aliases;

  // If the new response didn't have a vary header, we continue to use the
  // header from the stored response per the effect of headers->Update().
  // Update the data with the new/updated request headers.
  response_.vary_data.Init(*request_, *response_.headers);

  if (UpdateAndReportCacheability(*response_.headers)) {
    if (!entry_->IsDoomed()) {
      int ret = cache_->DoomEntry(cache_key_, nullptr);
      DCHECK_EQ(OK, ret);
    }
    TransitionToState(STATE_UPDATE_CACHED_RESPONSE_COMPLETE);
  } else {
    // If we are already reading, we already updated the headers for this
    // request; doing it again will change Content-Length.
    if (!reading_) {
      TransitionToState(STATE_CACHE_WRITE_UPDATED_RESPONSE);
      rv = OK;
    } else {
      TransitionToState(STATE_UPDATE_CACHED_RESPONSE_COMPLETE);
    }
  }

  return rv;
}

int HttpCache::Transaction::DoCacheWriteUpdatedResponse() {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoCacheWriteUpdatedResponse", track_for_state_change_);
  TransitionToState(STATE_CACHE_WRITE_UPDATED_RESPONSE_COMPLETE);
  return WriteResponseInfoToEntry(response_, false);
}

int HttpCache::Transaction::DoCacheWriteUpdatedResponseComplete(int result) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoCacheWriteUpdatedResponseComplete",
                      track_for_state_change_, "result", result);
  TransitionToState(STATE_UPDATE_CACHED_RESPONSE_COMPLETE);
  return OnWriteResponseInfoToEntryComplete(result);
}

int HttpCache::Transaction::DoUpdateCachedResponseComplete(int result) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoUpdateCachedResponseComplete", track_for_state_change_,
                      "result", result);
  if (mode_ == UPDATE) {
    DCHECK(!handling_206_);
    // We got a "not modified" response and already updated the corresponding
    // cache entry above.
    //
    // By stopping to write to the cache now, we make sure that the 304 rather
    // than the cached 200 response, is what will be returned to the user.
    UpdateSecurityHeadersBeforeForwarding();
    DoneWithEntry(true);
  } else if (entry_ && !handling_206_) {
    DCHECK_EQ(READ_WRITE, mode_);
    if ((!partial_ && !entry_->IsWritingInProgress()) ||
        (partial_ && partial_->IsLastRange())) {
      mode_ = READ;
    }
    // We no longer need the network transaction, so destroy it.
    if (network_trans_) {
      ResetNetworkTransaction();
    }
  } else if (entry_ && handling_206_ && truncated_ &&
             partial_->initial_validation()) {
    // We just finished the validation of a truncated entry, and the server
    // is willing to resume the operation. Now we go back and start serving
    // the first part to the user.
    if (network_trans_) {
      ResetNetworkTransaction();
    }
    new_response_ = nullptr;
    TransitionToState(STATE_START_PARTIAL_CACHE_VALIDATION);
    partial_->SetRangeToStartDownload();
    return OK;
  }
  TransitionToState(STATE_OVERWRITE_CACHED_RESPONSE);
  return OK;
}

int HttpCache::Transaction::DoOverwriteCachedResponse() {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoOverwriteCachedResponse", track_for_state_change_);
  if (mode_ & READ) {
    TransitionToState(STATE_PARTIAL_HEADERS_RECEIVED);
    return OK;
  }

  // We change the value of Content-Length for partial content.
  if (handling_206_ && partial_) {
    partial_->FixContentLength(new_response_->headers.get());
  }

  SetResponse(*new_response_);

  if (method_ == "HEAD") {
    // This response is replacing the cached one.
    DoneWithEntry(false);
    new_response_ = nullptr;
    TransitionToState(STATE_FINISH_HEADERS);
    return OK;
  }

  if (handling_206_ && !CanResume(false)) {
    // There is no point in storing this resource because it will never be used.
    // This may change if we support LOAD_ONLY_FROM_CACHE with sparse entries.
    DoneWithEntry(false);
    if (partial_) {
      partial_->FixResponseHeaders(response_.headers.get(), true);
    }
    TransitionToState(STATE_PARTIAL_HEADERS_RECEIVED);
    return OK;
  }
  // Mark the response with browser_run_id before it gets written.
  if (initial_request_->browser_run_id.has_value()) {
    response_.browser_run_id = initial_request_->browser_run_id;
  }

  TransitionToState(STATE_CACHE_WRITE_RESPONSE);
  return OK;
}

int HttpCache::Transaction::DoCacheWriteResponse() {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoCacheWriteResponse",
                      track_for_state_change_);
  DCHECK(response_.headers);
  // Invalidate any current entry with a successful response if this transaction
  // cannot write to this entry. This transaction then continues to read from
  // the network without writing to the backend.
  bool is_match = response_.headers->response_code() == HTTP_NOT_MODIFIED;
  if (entry_ && !entry_->CanTransactionWriteResponseHeaders(
                    this, partial_ != nullptr, is_match)) {
    done_headers_create_new_entry_ = true;

    // The transaction needs to overwrite this response. Doom the current entry,
    // create a new one (by going to STATE_INIT_ENTRY), and then jump straight
    // to writing out the response, bypassing the headers checks. The mode_ is
    // set to WRITE in order to doom any other existing entries that might exist
    // so that this transaction can go straight to writing a response.
    mode_ = WRITE;
    TransitionToState(STATE_INIT_ENTRY);
    cache_->DoomEntryValidationNoMatch(std::move(entry_));
    entry_.reset();
    return OK;
  }

  TransitionToState(STATE_CACHE_WRITE_RESPONSE_COMPLETE);
  return WriteResponseInfoToEntry(response_, truncated_);
}

int HttpCache::Transaction::DoCacheWriteResponseComplete(int result) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoCacheWriteResponseComplete", track_for_state_change_,
                      "result", result);
  TransitionToState(STATE_TRUNCATE_CACHED_DATA);
  return OnWriteResponseInfoToEntryComplete(result);
}

int HttpCache::Transaction::DoTruncateCachedData() {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoTruncateCachedData",
                      track_for_state_change_);
  TransitionToState(STATE_TRUNCATE_CACHED_DATA_COMPLETE);
  if (!entry_) {
    return OK;
  }
  net_log_.BeginEvent(NetLogEventType::HTTP_CACHE_WRITE_DATA);
  BeginDiskCacheAccessTimeCount();
  // Truncate the stream.
  return entry_->GetEntry()->WriteData(kResponseContentIndex, /*offset=*/0,
                                       /*buf=*/nullptr, /*buf_len=*/0,
                                       io_callback_, /*truncate=*/true);
}

int HttpCache::Transaction::DoTruncateCachedDataComplete(int result) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoTruncateCachedDataComplete", track_for_state_change_,
                      "result", result);
  EndDiskCacheAccessTimeCount(DiskCacheAccessType::kWrite);
  if (entry_) {
    net_log_.EndEventWithNetErrorCode(NetLogEventType::HTTP_CACHE_WRITE_DATA,
                                      result);
  }

  TransitionToState(STATE_PARTIAL_HEADERS_RECEIVED);
  return OK;
}

int HttpCache::Transaction::DoPartialHeadersReceived() {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoPartialHeadersReceived", track_for_state_change_);
  new_response_ = nullptr;

  if (partial_ && mode_ != NONE && !reading_) {
    // We are about to return the headers for a byte-range request to the user,
    // so let's fix them.
    partial_->FixResponseHeaders(response_.headers.get(), true);
  }
  TransitionToState(STATE_FINISH_HEADERS);
  return OK;
}

int HttpCache::Transaction::DoHeadersPhaseCannotProceed(int result) {
  // If its the Start state machine and it cannot proceed due to a cache
  // failure, restart this transaction.
  DCHECK(!reading_);

  // Reset before invoking SetRequest() which can reset the request info sent to
  // network transaction.
  if (network_trans_) {
    network_trans_.reset();
  }

  new_response_ = nullptr;

  SetRequest(net_log_);

  entry_.reset();
  new_entry_.reset();
  last_disk_cache_access_start_time_ = TimeTicks();

  // TODO(crbug.com/40772202): This should probably clear `response_`,
  // too, once things are fixed so it's safe to do so.

  // Bypass the cache for timeout scenario.
  if (result == ERR_CACHE_LOCK_TIMEOUT) {
    effective_load_flags_ |= LOAD_DISABLE_CACHE;
  }

  TransitionToState(STATE_GET_BACKEND);
  return OK;
}

int HttpCache::Transaction::DoFinishHeaders(int result) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoFinishHeaders",
                      track_for_state_change_, "result", result);
  if (!cache_.get() || !entry_ || result != OK) {
    TransitionToState(STATE_NONE);
    return result;
  }

  TransitionToState(STATE_FINISH_HEADERS_COMPLETE);

  // If it was an auth failure, this transaction should continue to be
  // headers_transaction till consumer takes an action, so no need to do
  // anything now.
  // TODO(crbug.com/40529460). See the issue for a suggestion for cleaning the
  // state machine to be able to remove this condition.
  if (auth_response_.headers.get()) {
    return OK;
  }

  // If the transaction needs to wait because another transaction is still
  // writing the response body, it will return ERR_IO_PENDING now and the
  // cache_io_callback_ will be invoked when the wait is done.
  int rv = cache_->DoneWithResponseHeaders(entry_, this, partial_ != nullptr);
  DCHECK(!reading_ || rv == OK) << "Expected OK, but got " << rv;

  if (rv == ERR_IO_PENDING) {
    DCHECK(entry_lock_waiting_since_.is_null());
    entry_lock_waiting_since_ = TimeTicks::Now();
    AddCacheLockTimeoutHandler(entry_.get());
  }
  return rv;
}

int HttpCache::Transaction::DoFinishHeadersComplete(int rv) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoFinishHeadersComplete", track_for_state_change_,
                      "result", rv);
  entry_lock_waiting_since_ = TimeTicks();
  if (rv == ERR_CACHE_RACE || rv == ERR_CACHE_LOCK_TIMEOUT) {
    TransitionToState(STATE_HEADERS_PHASE_CANNOT_PROCEED);
    return rv;
  }

  if (network_trans_ && InWriters()) {
    entry_->writers()->SetNetworkTransaction(this, std::move(network_trans_));
    moved_network_transaction_to_writers_ = true;
  }

  // If already reading, that means it is a partial request coming back to the
  // headers phase, continue to the appropriate reading state.
  if (reading_) {
    int reading_state_rv = TransitionToReadingState();
    DCHECK_EQ(OK, reading_state_rv);
    return OK;
  }

  TransitionToState(STATE_NONE);
  return rv;
}

int HttpCache::Transaction::DoNetworkReadCacheWrite() {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoNetworkReadCacheWrite", track_for_state_change_,
                      "read_offset", read_offset_, "read_buf_len",
                      read_buf_len_);
  DCHECK(InWriters());
  TransitionToState(STATE_NETWORK_READ_CACHE_WRITE_COMPLETE);
  return entry_->writers()->Read(read_buf_, read_buf_len_, io_callback_, this);
}

int HttpCache::Transaction::DoNetworkReadCacheWriteComplete(int result) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoNetworkReadCacheWriteComplete",
                      track_for_state_change_, "result", result);
  if (!cache_.get()) {
    TransitionToState(STATE_NONE);
    return ERR_UNEXPECTED;
  }
  // |result| will be error code in case of network read failure and |this|
  // cannot proceed further, so set entry_ to null. |result| will not be error
  // in case of cache write failure since |this| can continue to read from the
  // network. If response is completed, then also set entry to null.
  if (result < 0) {
    // We should have discovered this error in WriterAboutToBeRemovedFromEntry
    DCHECK_EQ(result, shared_writing_error_);
    DCHECK_EQ(NONE, mode_);
    DCHECK(!entry_);
    TransitionToState(STATE_NONE);
    return result;
  }

  if (partial_) {
    return DoPartialNetworkReadCompleted(result);
  }

  if (result == 0) {
    DCHECK_EQ(NONE, mode_);
    DCHECK(!entry_);
  } else {
    read_offset_ += result;
  }
  TransitionToState(STATE_NONE);
  return result;
}

int HttpCache::Transaction::DoPartialNetworkReadCompleted(int result) {
  DCHECK(partial_);

  // Go to the next range if nothing returned or return the result.
  // TODO(shivanisha) Simplify this condition if possible. It was introduced
  // in https://codereview.chromium.org/545101
  if (result != 0 || truncated_ ||
      !(partial_->IsLastRange() || mode_ == WRITE)) {
    partial_->OnNetworkReadCompleted(result);

    if (result == 0) {
      // We need to move on to the next range.
      if (network_trans_) {
        ResetNetworkTransaction();
      } else if (InWriters() && entry_->writers()->network_transaction()) {
        SaveNetworkTransactionInfo(*(entry_->writers()->network_transaction()));
        entry_->writers()->ResetNetworkTransaction();
      }
      TransitionToState(STATE_START_PARTIAL_CACHE_VALIDATION);
    } else {
      TransitionToState(STATE_NONE);
    }
    return result;
  }

  // Request completed.
  if (result == 0) {
    DoneWithEntry(true);
  }

  TransitionToState(STATE_NONE);
  return result;
}

int HttpCache::Transaction::DoNetworkRead() {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoNetworkRead",
                      track_for_state_change_, "read_offset", read_offset_,
                      "read_buf_len", read_buf_len_);
  TransitionToState(STATE_NETWORK_READ_COMPLETE);
  return network_trans_->Read(read_buf_.get(), read_buf_len_, io_callback_);
}

int HttpCache::Transaction::DoNetworkReadComplete(int result) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoNetworkReadComplete",
                      track_for_state_change_, "result", result);

  if (!cache_.get()) {
    TransitionToState(STATE_NONE);
    return ERR_UNEXPECTED;
  }

  if (partial_) {
    return DoPartialNetworkReadCompleted(result);
  }

  TransitionToState(STATE_NONE);
  return result;
}

int HttpCache::Transaction::DoCacheReadData() {
  if (entry_) {
    DCHECK(InWriters() || entry_->TransactionInReaders(this));
  }

  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoCacheReadData",
                      track_for_state_change_, "read_offset", read_offset_,
                      "read_buf_len", read_buf_len_);

  if (method_ == "HEAD") {
    TransitionToState(STATE_NONE);
    return 0;
  }

  DCHECK(entry_);
  TransitionToState(STATE_CACHE_READ_DATA_COMPLETE);

  net_log_.BeginEvent(NetLogEventType::HTTP_CACHE_READ_DATA);
  if (partial_) {
    return partial_->CacheRead(entry_->GetEntry(), read_buf_.get(),
                               read_buf_len_, io_callback_);
  }

  RecordEntrySizeHistograms(*entry_->GetEntry());

  BeginDiskCacheAccessTimeCount();
  return entry_->GetEntry()->ReadData(kResponseContentIndex, read_offset_,
                                      read_buf_.get(), read_buf_len_,
                                      io_callback_);
}

int HttpCache::Transaction::DoCacheReadDataComplete(int result) {
  EndDiskCacheAccessTimeCount(DiskCacheAccessType::kRead);
  if (entry_) {
    DCHECK(InWriters() || entry_->TransactionInReaders(this));
  }

  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "DoCacheReadDataComplete", track_for_state_change_,
                      "result", result);
  net_log_.EndEventWithNetErrorCode(NetLogEventType::HTTP_CACHE_READ_DATA,
                                    result);

  if (!cache_.get()) {
    TransitionToState(STATE_NONE);
    return ERR_UNEXPECTED;
  }

  if (partial_) {
    // Partial requests are confusing to report in histograms because they may
    // have multiple underlying requests.
    UpdateCacheEntryStatusToOther(OtherStatusReason::kPartialRequest);
    return DoPartialCacheReadCompleted(result);
  }

  if (result > 0) {
    read_offset_ += result;
  } else if (result == 0) {  // End of file.
    DoneWithEntry(true);
  } else {
    return OnCacheReadError(result, false);
  }

  TransitionToState(STATE_NONE);
  return result;
}

//-----------------------------------------------------------------------------

void HttpCache::Transaction::SetRequest(const NetLogWithSource& net_log) {
  net_log_ = net_log;

  // Reset the variables that might get set in this function. This is done
  // because this function can be invoked multiple times for a transaction.
  cache_entry_status_ = CacheEntryStatus::ENTRY_UNDEFINED;
  external_validation_.reset();
  range_requested_ = false;
  partial_.reset();

  request_ = initial_request_;
  mutable_request_.reset();
  if (no_vary_search_cache_erase_handle_) {
    net_log_.EndEvent(
        NetLogEventType::HTTP_CACHE_USING_NO_VARY_SEARCH_CACHE_URL);
    no_vary_search_cache_erase_handle_.reset();
  }

  effective_load_flags_ = request_->load_flags;
  method_ = request_->method;

  if (cache_->mode() == DISABLE) {
    effective_load_flags_ |= LOAD_DISABLE_CACHE;
  }

  bool range_found =
      request_->extra_headers.HasHeader(HttpRequestHeaders::kRange);
  int load_flags_for_extra_headers =
      http_cache_util::GetLoadFlagsForExtraHeaders(request_->extra_headers);
  effective_load_flags_ |= load_flags_for_extra_headers;

  base::expected<std::optional<http_cache_util::ValidationHeaders>,
                 std::string_view>
      maybe_validation_headers =
          http_cache_util::ValidationHeaders::MaybeCreate(
              request_->extra_headers);
  std::optional<std::string_view> external_validation_error;
  if (maybe_validation_headers.has_value()) {
    external_validation_ = std::move(maybe_validation_headers.value());
  } else {
    external_validation_error = maybe_validation_headers.error();
  }

  if (range_found || load_flags_for_extra_headers || external_validation_ ||
      external_validation_error) {
    // Log the headers before request_ is modified.
    std::string empty;
    NetLogRequestHeaders(net_log_,
                         NetLogEventType::HTTP_CACHE_CALLER_REQUEST_HEADERS,
                         empty, &request_->extra_headers);
  }

  // We don't support ranges and validation headers.
  if (range_found && external_validation_) {
    LOG(WARNING) << "Byte ranges AND validation headers found.";
    effective_load_flags_ |= LOAD_DISABLE_CACHE;
  }

  // If there is an invalid validation header, we can't treat this request as
  // a cache validation.
  if (external_validation_error) {
    LOG(WARNING) << *external_validation_error;
    effective_load_flags_ |= LOAD_DISABLE_CACHE;
  }

  if (range_found && !(effective_load_flags_ & LOAD_DISABLE_CACHE)) {
    UpdateCacheEntryStatusToOther(OtherStatusReason::kRangeHeaderFound);
    partial_ = std::make_unique<PartialData>();
    if (method_ == "GET" && partial_->Init(request_->extra_headers)) {
      // We will be modifying the actual range requested to the server, so
      // let's remove the header here.
      // Note that mutable_request_ is a shallow copy so will keep the same
      // pointer to upload data stream as in the original request.
      EnsureMutableRequest();
      mutable_request_->extra_headers.RemoveHeader(HttpRequestHeaders::kRange);
      partial_->SetHeaders(mutable_request_->extra_headers);
    } else {
      // The range is invalid or we cannot handle it properly.
      VLOG(1) << "Invalid byte range found.";
      effective_load_flags_ |= LOAD_DISABLE_CACHE;
      partial_.reset(nullptr);
    }
  }
}

bool HttpCache::Transaction::ShouldPassThrough() {
  // We may have a null disk_cache if there is an error we cannot recover from,
  // like not enough disk space, or sharing violations.
  if (!cache_->disk_cache_.get()) {
    return true;
  }

  if (effective_load_flags_ & LOAD_DISABLE_CACHE) {
    return true;
  }

  if (method_ == "GET" || method_ == "HEAD") {
    return false;
  }

  if (method_ == "POST" && request_->upload_data_stream &&
      request_->upload_data_stream->identifier()) {
    return false;
  }

  if (method_ == "PUT" && request_->upload_data_stream) {
    return false;
  }

  // DELETE and PATCH requests may result in invalidating the cache, so cannot
  // just pass through.
  if (method_ == "DELETE" || method_ == "PATCH") {
    return false;
  }

  return true;
}

int HttpCache::Transaction::BeginCacheRead() {
  // We don't support any combination of LOAD_ONLY_FROM_CACHE and byte ranges.
  // It's possible to trigger this from JavaScript using the Fetch API with
  // `cache: 'only-if-cached'` so ideally we should support it.
  // TODO(ricea): Correctly read from the cache in this case.
  if (response_.headers->response_code() == HTTP_PARTIAL_CONTENT || partial_) {
    TransitionToState(STATE_FINISH_HEADERS);
    return ERR_CACHE_MISS;
  }

  // We don't have the whole resource.
  if (truncated_) {
    TransitionToState(STATE_FINISH_HEADERS);
    return ERR_CACHE_MISS;
  }

  if (RequiresValidation() != VALIDATION_NONE) {
    if (IsUsingURLFromNoVarySearchCache()) {
      // A future transaction that is not read-only may be able to use this
      // entry, so don't remove it from the cache.
      return RestartWithoutNoVarySearchCache(
          RestartCacheEntryAction::kDontErase,
          NoVarySearchUseResult::kReadOnlyNeedsValidation);
    }
    TransitionToState(STATE_FINISH_HEADERS);
    return ERR_CACHE_MISS;
  }

  if (method_ == "HEAD") {
    FixHeadersForHead();
  }

  TransitionToState(STATE_FINISH_HEADERS);
  return OK;
}

int HttpCache::Transaction::BeginCacheValidation() {
  DCHECK_EQ(mode_, READ_WRITE);

  ValidationType required_validation = RequiresValidation();

  bool skip_validation = (required_validation == VALIDATION_NONE);
  bool needs_stale_while_revalidate_cache_update = false;
  const bool incomplete_body =
      truncated_ || response_.headers->response_code() == HTTP_PARTIAL_CONTENT;

  if (IsUsingURLFromNoVarySearchCache() && incomplete_body) {
    // Avoid using the No-Vary-Search cache in partial content situations.
    return RestartWithoutNoVarySearchCache(
        RestartCacheEntryAction::kErase,
        NoVarySearchUseResult::kIncompleteBody);
  }

  // Handle Stale-While-Revalidate if the client supports it.
  // This is not done for truncated entries since they need validation in order
  // to figure out how to deal with the missing part.
  if ((effective_load_flags_ & LOAD_SUPPORT_ASYNC_REVALIDATION) &&
      required_validation == VALIDATION_ASYNCHRONOUS && !truncated_) {
    DCHECK_EQ(request_->method, "GET");
    skip_validation = true;
    response_.async_revalidation_requested = true;
    needs_stale_while_revalidate_cache_update =
        response_.stale_revalidate_timeout.is_null();
  }

  if (method_ == "HEAD" && incomplete_body) {
    DCHECK(!partial_);
    if (skip_validation) {
      DCHECK(!reading_);
      TransitionToState(STATE_CONNECTED_CALLBACK);
      return OK;
    }

    // Bail out!
    TransitionToState(STATE_SEND_REQUEST);
    mode_ = NONE;
    return OK;
  }

  if (truncated_) {
    // Truncated entries can cause partial gets, so we shouldn't record this
    // load in histograms.
    UpdateCacheEntryStatusToOther(OtherStatusReason::kTruncatedEntry);
    skip_validation = !partial_->initial_validation();
  }

  // If this is the first request (!reading_) of a 206 entry (is_sparse_) that
  // doesn't actually cover the entire file (which with !reading would require
  // partial->IsLastRange()), and the user is requesting the whole thing
  // (!partial_->range_requested()), make sure to validate the first chunk,
  // since afterwards it will be too late if it's actually out-of-date (or the
  // server bungles invalidation). This is limited to the whole-file request
  // as a targeted fix for https://crbug.com/888742 while avoiding extra
  // requests in other cases, but the problem can occur more generally as well;
  // it's just a lot less likely with applications actively using ranges.
  // See https://crbug.com/902724 for the more general case.
  bool first_read_of_full_from_partial =
      is_sparse_ && !reading_ &&
      (partial_ && !partial_->range_requested() && !partial_->IsLastRange());

  if (partial_ && (is_sparse_ || truncated_) &&
      (!partial_->IsCurrentRangeCached() || invalid_range_ ||
       first_read_of_full_from_partial)) {
    // Force revalidation for sparse or truncated entries. Note that we don't
    // want to ignore the regular validation logic just because a byte range was
    // part of the request.
    skip_validation = false;
  }

  if (skip_validation) {
    UpdateCacheEntryStatus(CacheEntryStatus::ENTRY_USED);
    DCHECK(!reading_);
    TransitionToState(needs_stale_while_revalidate_cache_update
                          ? STATE_CACHE_UPDATE_STALE_WHILE_REVALIDATE_TIMEOUT
                          : STATE_CONNECTED_CALLBACK);
    return OK;
  } else {
    // Make the network request conditional, to see if we may reuse our cached
    // response.  If we cannot do so, then we just resort to a normal fetch.
    // Our mode remains READ_WRITE for a conditional request.  Even if the
    // conditionalization fails, we don't switch to WRITE mode until we
    // know we won't be falling back to using the cache entry in the
    // LOAD_FROM_CACHE_IF_OFFLINE case.
    if (!ConditionalizeRequest()) {
      couldnt_conditionalize_request_ = true;
      if (cache_entry_status_ != CacheEntryStatus::ENTRY_CANT_CONDITIONALIZE) {
        // `cache_entry_status_` may already be marked as
        // `ENTRY_CANT_CONDITIONALIZE`. This can occur if an existed cache entry
        // was initially deemed unusable (e.g. a "no-cache" header), and then,
        // another concurrent same URL transactions receives the "no-cache"
        // response again which leads this transaction's BeginCacheValidation()
        // to re-evaluate the entry as unusable. This check avoids redundant
        // status updates.
        UpdateCacheEntryStatus(CacheEntryStatus::ENTRY_CANT_CONDITIONALIZE);
      }
      if (partial_) {
        return DoRestartPartialRequest();
      }
      if (IsUsingURLFromNoVarySearchCache()) {
        // We shouldn't send an unconditional request for the original URL, so
        // restart the transaction. However, don't remove the cache entry, as it
        // might still be usable by a future request with the
        // LOAD_SKIP_CACHE_VALIDATION flag.
        return RestartWithoutNoVarySearchCache(
            RestartCacheEntryAction::kDontErase,
            NoVarySearchUseResult::kCouldntConditionalize);
      }

      DCHECK_NE(HTTP_PARTIAL_CONTENT, response_.headers->response_code());
    }
    TransitionToState(STATE_SEND_REQUEST);
  }
  return OK;
}

int HttpCache::Transaction::BeginPartialCacheValidation() {
  DCHECK_EQ(mode_, READ_WRITE);

  if (response_.headers->response_code() != HTTP_PARTIAL_CONTENT && !partial_ &&
      !truncated_) {
    return BeginCacheValidation();
  }

  // Partial requests should not be recorded in histograms.
  UpdateCacheEntryStatusToOther(OtherStatusReason::kPartialValidation);
  if (method_ == "HEAD") {
    return BeginCacheValidation();
  }

  if (!range_requested_) {
    // The request is not for a range, but we have stored just ranges.

    partial_ = std::make_unique<PartialData>();
    partial_->SetHeaders(request_->extra_headers);
    EnsureMutableRequest();
  }

  TransitionToState(STATE_CACHE_QUERY_DATA);
  return OK;
}

// This should only be called once per request.
int HttpCache::Transaction::ValidateEntryHeadersAndContinue() {
  DCHECK_EQ(mode_, READ_WRITE);

  if (!partial_->UpdateFromStoredHeaders(response_.headers.get(),
                                         entry_->GetEntry(), truncated_,
                                         entry_->IsWritingInProgress())) {
    return DoRestartPartialRequest();
  }

  if (response_.headers->response_code() == HTTP_PARTIAL_CONTENT) {
    is_sparse_ = true;
  }

  if (!partial_->IsRequestedRangeOK()) {
    // The stored data is fine, but the request may be invalid.
    invalid_range_ = true;
  }

  TransitionToState(STATE_START_PARTIAL_CACHE_VALIDATION);
  return OK;
}

int HttpCache::Transaction::BeginExternallyConditionalizedRequest() {
  DCHECK_EQ(UPDATE, mode_);
  CHECK(external_validation_);

  if (response_.headers->response_code() != HTTP_OK || truncated_ ||
      !external_validation_->Match(*response_.headers)) {
    // The externally conditionalized request is not a validation request
    // for our existing cache entry. Proceed with caching disabled.
    UpdateCacheEntryStatusToOther(OtherStatusReason::kPreConditionalized);
    DoneWithEntry(true);
  }

  TransitionToState(STATE_SEND_REQUEST);
  return OK;
}

int HttpCache::Transaction::RestartNetworkRequest() {
  DCHECK(mode_ & WRITE || mode_ == NONE);
  DCHECK(network_trans_.get());
  DCHECK_EQ(STATE_NONE, next_state_);

  next_state_ = STATE_SEND_REQUEST_COMPLETE;
  int rv = network_trans_->RestartIgnoringLastError(io_callback_);
  if (rv != ERR_IO_PENDING) {
    return DoLoop(rv);
  }
  return rv;
}

int HttpCache::Transaction::RestartNetworkRequestWithCertificate(
    scoped_refptr<X509Certificate> client_cert,
    scoped_refptr<SSLPrivateKey> client_private_key) {
  DCHECK(mode_ & WRITE || mode_ == NONE);
  DCHECK(network_trans_.get());
  DCHECK_EQ(STATE_NONE, next_state_);

  next_state_ = STATE_SEND_REQUEST_COMPLETE;
  int rv = network_trans_->RestartWithCertificate(
      std::move(client_cert), std::move(client_private_key), io_callback_);
  if (rv != ERR_IO_PENDING) {
    return DoLoop(rv);
  }
  return rv;
}

int HttpCache::Transaction::RestartNetworkRequestWithAuth(
    const AuthCredentials& credentials) {
  DCHECK(mode_ & WRITE || mode_ == NONE);
  DCHECK(network_trans_.get());
  DCHECK_EQ(STATE_NONE, next_state_);

  next_state_ = STATE_SEND_REQUEST_COMPLETE;
  int rv = network_trans_->RestartWithAuth(credentials, io_callback_);
  if (rv != ERR_IO_PENDING) {
    return DoLoop(rv);
  }
  return rv;
}

ValidationType HttpCache::Transaction::RequiresValidation() {
  // TODO(darin): need to do more work here:
  //  - make sure we have a matching request method
  //  - watch out for cached responses that depend on authentication

  if (response_.vary_data.is_valid() &&
      !response_.vary_data.MatchesRequest(*request_,
                                          *response_.headers.get())) {
    vary_mismatch_ = true;
    return VALIDATION_SYNCHRONOUS;
  }

  if (effective_load_flags_ & LOAD_SKIP_CACHE_VALIDATION) {
    return VALIDATION_NONE;
  }

  if (method_ == "PUT" || method_ == "DELETE" || method_ == "PATCH") {
    return VALIDATION_SYNCHRONOUS;
  }

  bool validate_flag = effective_load_flags_ & LOAD_VALIDATE_CACHE;

  ValidationType validation_required_by_headers =
      validate_flag ? VALIDATION_SYNCHRONOUS
                    : response_.headers->RequiresValidation(
                          response_.request_time, response_.response_time,
                          cache_->clock_->Now());

  if (validate_flag) {
    return VALIDATION_SYNCHRONOUS;
  }

  if (validation_required_by_headers == VALIDATION_ASYNCHRONOUS) {
    // Asynchronous revalidation is only supported for GET methods.
    if (request_->method != "GET") {
      return VALIDATION_SYNCHRONOUS;
    }

    // If the timeout on the staleness revalidation is set don't hand out
    // a resource that hasn't been async validated.
    if (!response_.stale_revalidate_timeout.is_null() &&
        response_.stale_revalidate_timeout < cache_->clock_->Now()) {
      return VALIDATION_SYNCHRONOUS;
    }
  }

  return validation_required_by_headers;
}

bool HttpCache::Transaction::IsResponseConditionalizable(
    std::string* etag_value,
    std::string* last_modified_value) const {
  DCHECK(response_.headers.get());

  // This only makes sense for cached 200 or 206 responses.
  if (response_.headers->response_code() != HTTP_OK &&
      response_.headers->response_code() != HTTP_PARTIAL_CONTENT) {
    return false;
  }

  // Just use the first available ETag and/or Last-Modified header value.
  // TODO(darin): Or should we use the last?

  if (response_.headers->GetHttpVersion() >= HttpVersion(1, 1)) {
    response_.headers->EnumerateHeader(nullptr, "etag", etag_value);
  }

  response_.headers->EnumerateHeader(nullptr, "last-modified",
                                     last_modified_value);

  if (etag_value->empty() && last_modified_value->empty()) {
    return false;
  }

  return true;
}

bool HttpCache::Transaction::ShouldOpenOnlyMethods() const {
  // These methods indicate that we should only try to open an entry and not
  // fallback to create.
  return method_ == "PUT" || method_ == "DELETE" || method_ == "PATCH" ||
         (method_ == "HEAD" && mode_ == READ_WRITE);
}

bool HttpCache::Transaction::ConditionalizeRequest() {
  DCHECK(response_.headers.get());

  if (method_ == "PUT" || method_ == "DELETE" || method_ == "PATCH") {
    return false;
  }

  if (fail_conditionalization_for_test_) {
    return false;
  }

  std::string etag_value;
  std::string last_modified_value;
  if (!IsResponseConditionalizable(&etag_value, &last_modified_value)) {
    return false;
  }

  DCHECK(response_.headers->response_code() != HTTP_PARTIAL_CONTENT ||
         response_.headers->HasStrongValidators());

  if (vary_mismatch_) {
    // Can't rely on last-modified if vary is different.
    last_modified_value.clear();
    if (etag_value.empty()) {
      return false;
    }
  }

  // We will need to modify `request_` to change the headers, so allocate
  // `mutable_request_` if it has not been allocated already.
  EnsureMutableRequest();

  bool use_if_range =
      partial_ && !partial_->IsCurrentRangeCached() && !invalid_range_;

  if (!etag_value.empty()) {
    if (use_if_range) {
      // We don't want to switch to WRITE mode if we don't have this block of a
      // byte-range request because we may have other parts cached.
      mutable_request_->extra_headers.SetHeader(HttpRequestHeaders::kIfRange,
                                                etag_value);
    } else {
      mutable_request_->extra_headers.SetHeader(
          HttpRequestHeaders::kIfNoneMatch, etag_value);
    }
    // For byte-range requests, make sure that we use only one way to validate
    // the request.
    if (partial_ && !partial_->IsCurrentRangeCached()) {
      return true;
    }
  }

  if (!last_modified_value.empty()) {
    if (use_if_range) {
      mutable_request_->extra_headers.SetHeader(HttpRequestHeaders::kIfRange,
                                                last_modified_value);
    } else {
      mutable_request_->extra_headers.SetHeader(
          HttpRequestHeaders::kIfModifiedSince, last_modified_value);
    }
  }

  return true;
}

HttpCache::Transaction::HttpCacheEntryRejectionStatus
HttpCache::Transaction::GetHttpCacheEntryRejectionStatus(
    uint8_t in_memory_info) {
  // Not going to be clever with those...
  if (partial_) {
    return HttpCacheEntryRejectionStatus::kNoRejectionPartial;
  }

  // Avoiding open based on in-memory hints requires us to be permitted to
  // modify the cache, including deleting an old entry. Only the READ_WRITE
  // and WRITE modes permit that... and WRITE never tries to open entries in the
  // first place, so we shouldn't see it here.
  DCHECK_NE(mode_, WRITE);
  if (mode_ != READ_WRITE) {
    return HttpCacheEntryRejectionStatus::kNoRejectionNonReadWriteMode;
  }

  // If we are loading ignoring cache validity (aka back button), obviously
  // can't reject things based on it.  Also if LOAD_ONLY_FROM_CACHE there is no
  // hope of network offering anything better.
  if (effective_load_flags_ & LOAD_SKIP_CACHE_VALIDATION) {
    return HttpCacheEntryRejectionStatus::kNoRejectionSkipCacheValidation;
  }

  if (effective_load_flags_ & LOAD_ONLY_FROM_CACHE) {
    return HttpCacheEntryRejectionStatus::kNoRejectionLoadOnlyFromCache;
  }

  if ((in_memory_info & HINT_UNUSABLE_PER_CACHING_HEADERS) !=
      HINT_UNUSABLE_PER_CACHING_HEADERS) {
    return HttpCacheEntryRejectionStatus::kNoRejectionUsable;
  }

  return base::FeatureList::IsEnabled(features::kHttpCacheSkipUnusableEntry)
             ? HttpCacheEntryRejectionStatus::kRejection
             : HttpCacheEntryRejectionStatus::kNoRejectionHintDisabled;
}

bool HttpCache::Transaction::MaybeRejectBasedOnEntryInMemoryData(
    uint8_t in_memory_info) {
  HttpCacheEntryRejectionStatus status =
      GetHttpCacheEntryRejectionStatus(in_memory_info);
  UMA_HISTOGRAM_ENUMERATION("HttpCache.EntryRejectionStatus", status);
  if (base::FeatureList::IsEnabled(
          features::kUpdateIsMainFrameOriginRecentlyAccessed)) {
    if (effective_load_flags_ & LOAD_IS_MAIN_FRAME_ORIGIN_RECENTLY_ACCESSED) {
      UMA_HISTOGRAM_ENUMERATION(
          "HttpCache.EntryRejectionStatus."
          "MainFrameOriginRecentlyAccessed",
          status);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "HttpCache.EntryRejectionStatus."
          "MainFrameOriginNotRecentlyAccessed",
          status);
    }
  }

  return status == HttpCacheEntryRejectionStatus::kRejection;
}

bool HttpCache::Transaction::ComputeUnusablePerCachingHeaders() {
  // unused_since_prefetch overrides some caching headers, so it may be useful
  // regardless of what they say.
  if (response_.unused_since_prefetch) {
    return false;
  }

  // Has an e-tag or last-modified: we can probably send a conditional request,
  // so it's potentially useful.
  std::string etag_ignored, last_modified_ignored;
  if (IsResponseConditionalizable(&etag_ignored, &last_modified_ignored)) {
    return false;
  }

  // If none of the above is true and the entry has zero freshness and
  // no stale-while-revaliate, then it won't be usable absent load flag
  // override.
  auto freshness_lifetimes =
      response_.headers->GetFreshnessLifetimes(response_.response_time);

  if (!recorded_response_freshness_is_zero_) {
    base::UmaHistogramBoolean("HttpCache.ResponseFreshnessIsZero",
                              freshness_lifetimes.freshness.is_zero());
    recorded_response_freshness_is_zero_ = true;
  }

  return freshness_lifetimes.freshness.is_zero() &&
         freshness_lifetimes.staleness.is_zero();
}

// We just received some headers from the server. We may have asked for a range,
// in which case partial_ has an object. This could be the first network request
// we make to fulfill the original request, or we may be already reading (from
// the net and / or the cache). If we are not expecting a certain response, we
// just bypass the cache for this request (but again, maybe we are reading), and
// delete partial_ (so we are not able to "fix" the headers that we return to
// the user). This results in either a weird response for the caller (we don't
// expect it after all), or maybe a range that was not exactly what it was asked
// for.
//
// If the server is simply telling us that the resource has changed, we delete
// the cached entry and restart the request as the caller intended (by returning
// false from this method). However, we may not be able to do that at any point,
// for instance if we already returned the headers to the user.
//
// WARNING: Whenever this code returns false, it has to make sure that the next
// time it is called it will return true so that we don't keep retrying the
// request.
bool HttpCache::Transaction::ValidatePartialResponse() {
  const HttpResponseHeaders* headers = new_response_->headers.get();
  int response_code = headers->response_code();
  bool partial_response = (response_code == HTTP_PARTIAL_CONTENT);
  handling_206_ = false;

  if (!entry_ || method_ != "GET") {
    return true;
  }

  if (invalid_range_) {
    // We gave up trying to match this request with the stored data. If the
    // server is ok with the request, delete the entry, otherwise just ignore
    // this request
    DCHECK(!reading_);
    if (partial_response || response_code == HTTP_OK) {
      DoomPartialEntry(true);
      CHECK(!IsUsingURLFromNoVarySearchCache());
      mode_ = NONE;
    } else {
      if (response_code == HTTP_NOT_MODIFIED) {
        // Change the response code of the request to be 416 (Requested range
        // not satisfiable).
        SetResponse(*new_response_);
        partial_->FixResponseHeaders(response_.headers.get(), false);
      }
      IgnoreRangeRequest();
    }
    return true;
  }

  if (!partial_) {
    // We are not expecting 206 but we may have one.
    if (partial_response) {
      IgnoreRangeRequest();
    }

    return true;
  }

  // TODO(rvargas): Do we need to consider other results here?.
  bool failure = response_code == HTTP_OK ||
                 response_code == HTTP_REQUESTED_RANGE_NOT_SATISFIABLE;

  if (partial_->IsCurrentRangeCached()) {
    // We asked for "If-None-Match: " so a 206 means a new object.
    if (partial_response) {
      failure = true;
    }

    if (response_code == HTTP_NOT_MODIFIED &&
        partial_->ResponseHeadersOK(headers)) {
      return true;
    }
  } else {
    // We asked for "If-Range: " so a 206 means just another range.
    if (partial_response) {
      if (partial_->ResponseHeadersOK(headers)) {
        handling_206_ = true;
        return true;
      } else {
        failure = true;
      }
    }

    if (!reading_ && !is_sparse_ && !partial_response) {
      // See if we can ignore the fact that we issued a byte range request.
      // If the server sends 200, just store it. If it sends an error, redirect
      // or something else, we may store the response as long as we didn't have
      // anything already stored.
      if (response_code == HTTP_OK ||
          (!truncated_ && response_code != HTTP_NOT_MODIFIED &&
           response_code != HTTP_REQUESTED_RANGE_NOT_SATISFIABLE)) {
        // The server is sending something else, and we can save it.
        DCHECK((truncated_ && !partial_->IsLastRange()) || range_requested_);
        partial_.reset();
        truncated_ = false;
        return true;
      }
    }

    // 304 is not expected here, but we'll spare the entry (unless it was
    // truncated).
    if (truncated_) {
      failure = true;
    }
  }

  if (failure) {
    // We cannot truncate this entry, it has to be deleted.
    UpdateCacheEntryStatusToOther(OtherStatusReason::kValidatePartial);
    mode_ = NONE;
    if (is_sparse_ || truncated_) {
      // There was something cached to start with, either sparsed data (206), or
      // a truncated 200, which means that we probably modified the request,
      // adding a byte range or modifying the range requested by the caller.
      if (!reading_ && !partial_->IsLastRange()) {
        // We have not returned anything to the caller yet so it should be safe
        // to issue another network request, this time without us messing up the
        // headers.
        ResetPartialState(true);
        return false;
      }
      LOG(WARNING) << "Failed to revalidate partial entry";
    }
    DoomPartialEntry(true);
    return true;
  }

  IgnoreRangeRequest();
  return true;
}

void HttpCache::Transaction::IgnoreRangeRequest() {
  // We have a problem. We may or may not be reading already (in which case we
  // returned the headers), but we'll just pretend that this request is not
  // using the cache and see what happens. Most likely this is the first
  // response from the server (it's not changing its mind midway, right?).
  UpdateCacheEntryStatusToOther(OtherStatusReason::kIgnoreRangeRequest);
  DoneWithEntry(mode_ != WRITE);
  partial_.reset(nullptr);
}

// Called to signal to the consumer that we are about to read headers from a
// cached entry originally read from a given IP endpoint.
int HttpCache::Transaction::DoConnectedCallback() {
  TransitionToState(STATE_CONNECTED_CALLBACK_COMPLETE);
  if (connected_callback_.is_null()) {
    return OK;
  }

  auto type = response_.WasFetchedViaProxy() ? TransportType::kCachedFromProxy
                                             : TransportType::kCached;
  return connected_callback_.Run(
      TransportInfo(type, response_.remote_endpoint, /*accept_ch_frame_arg=*/"",
                    /*cert_is_issued_by_known_root=*/false,
                    NextProto::kProtoUnknown),
      io_callback_);
}

int HttpCache::Transaction::DoConnectedCallbackComplete(int result) {
  if (result != OK) {
    if (result ==
        ERR_CACHED_IP_ADDRESS_SPACE_BLOCKED_BY_LOCAL_NETWORK_ACCESS_POLICY) {
      net_log_.AddEvent(
          net::NetLogEventType::LOCAL_NETWORK_ACCESS_RETRY_DUE_TO_CACHE);
      DoomInconsistentEntry();
      UpdateCacheEntryStatusToOther(OtherStatusReason::kBlockedByIpSpace);
      TransitionToState(reading_ ? STATE_SEND_REQUEST
                                 : STATE_HEADERS_PHASE_CANNOT_PROCEED);
      return OK;
    }

    if (result == ERR_INCONSISTENT_IP_ADDRESS_SPACE) {
      DoomInconsistentEntry();
    } else {
      // Release the entry for further use - we are done using it.
      DoneWithEntry(/*entry_is_complete=*/true);
    }

    TransitionToState(STATE_NONE);
    return result;
  }

  if (reading_) {
    // We can only get here if we're reading a partial range of bytes from the
    // cache. In that case, proceed to read the bytes themselves.
    DCHECK(partial_);
    TransitionToState(STATE_CACHE_READ_DATA);
  } else {
    // Otherwise, we have just read headers from the cache.
    TransitionToState(STATE_SETUP_ENTRY_FOR_READ);
  }
  return OK;
}

void HttpCache::Transaction::DoomInconsistentEntry() {
  // Explicitly call `DoomActiveEntry()` ourselves before calling
  // `DoneWithEntry()` because we cannot rely on the latter doing it for us.
  // Indeed, `DoneWithEntry(false)` does not call `DoomActiveEntry()` if either
  // of the following conditions hold:
  //
  //  - the transaction uses the cache in read-only mode
  //  - the transaction has passed the headers phase and is reading
  //
  // Inconsistent cache entries can cause deterministic failures even in
  // read-only mode, so they should be doomed anyway. They can also be detected
  // during the reading phase in the case of split range requests, since those
  // requests can result in multiple connections being obtained to different
  // remote endpoints.
  cache_->DoomActiveEntry(cache_key_);
  DoneWithEntry(/*entry_is_complete=*/false);
}

void HttpCache::Transaction::FixHeadersForHead() {
  if (response_.headers->response_code() == HTTP_PARTIAL_CONTENT) {
    response_.headers->RemoveHeader("Content-Range");
    response_.headers->ReplaceStatusLine("HTTP/1.1 200 OK");
  }
}

int HttpCache::Transaction::DoSetupEntryForRead() {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoSetupEntryForRead",
                      track_for_state_change_);
  if (network_trans_) {
    ResetNetworkTransaction();
  }

  if (!entry_) {
    // Entry got destroyed when twiddling SWR bits.
    TransitionToState(STATE_HEADERS_PHASE_CANNOT_PROCEED);
    return OK;
  }

  if (partial_) {
    if (truncated_ || is_sparse_ ||
        (!invalid_range_ &&
         (response_.headers->response_code() == HTTP_OK ||
          response_.headers->response_code() == HTTP_PARTIAL_CONTENT))) {
      // We are going to return the saved response headers to the caller, so
      // we may need to adjust them first. In cases we are handling a range
      // request to a regular entry, we want the response to be a 200 or 206,
      // since others can't really be turned into a 206.
      TransitionToState(STATE_PARTIAL_HEADERS_RECEIVED);
      return OK;
    } else {
      partial_.reset();
    }
  }

  if (!entry_->IsWritingInProgress()) {
    mode_ = READ;
  }

  if (method_ == "HEAD") {
    FixHeadersForHead();
  }

  TransitionToState(STATE_FINISH_HEADERS);
  return OK;
}

int HttpCache::Transaction::WriteResponseInfoToEntry(
    const HttpResponseInfo& response,
    bool truncated) {
  DCHECK(response.headers);
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "WriteResponseInfoToEntry", track_for_state_change_,
                      "truncated", truncated);

  if (!entry_) {
    return OK;
  }

  net_log_.BeginEvent(NetLogEventType::HTTP_CACHE_WRITE_INFO);

  // Do not cache content with cert errors. This is to prevent not reporting net
  // errors when loading a resource from the cache.  When we load a page over
  // HTTPS with a cert error we show an SSL blocking page.  If the user clicks
  // proceed we reload the resource ignoring the errors.  The loaded resource is
  // then cached.  If that resource is subsequently loaded from the cache, no
  // net error is reported (even though the cert status contains the actual
  // errors) and no SSL blocking page is shown.  An alternative would be to
  // reverse-map the cert status to a net error and replay the net error.
  if (IsCertStatusError(response.ssl_info.cert_status) ||
      UpdateAndReportCacheability(*response.headers)) {
    if (partial_) {
      partial_->FixResponseHeaders(response_.headers.get(), true);
    }

    bool stopped = StopCachingImpl(false);
    DCHECK(stopped);
    net_log_.EndEventWithNetErrorCode(NetLogEventType::HTTP_CACHE_WRITE_INFO,
                                      OK);
    return OK;
  }

  if (truncated) {
    DCHECK_EQ(HTTP_OK, response.headers->response_code());
  }

  // If we get here, we are definitely going to write the entry, so update the
  // NoVarySearchCache in the case that there is a No-Vary-Search response
  // header.
  if (!truncated && !entry_->IsDoomed() && cache_->no_vary_search_cache_ &&
      IsNoVarySearchApplicable()) {
    cache_->no_vary_search_cache_->MaybeInsert(*request_, *response.headers);
  }

  // When writing headers, we only write the non-transient headers.
  static constexpr bool kSkipTransientHeaders = true;
  auto data = base::MakeRefCounted<PickledIOBuffer>(
      response.MakePickle(kSkipTransientHeaders, truncated));

  io_buf_len_ = data->size();

  // Summarize some info on cacheability in memory. Don't do it if doomed
  // since then |entry_| isn't definitive for |cache_key_|.
  if (!entry_->IsDoomed()) {
    uint8_t in_memory_data = 0;
    if (ComputeUnusablePerCachingHeaders()) {
      in_memory_data |= HINT_UNUSABLE_PER_CACHING_HEADERS;
    }
    if (request_->is_main_frame_navigation) {
      in_memory_data |= HINT_HIGH_PRIORITY;
    }
    entry_->GetEntry()->SetEntryInMemoryData(in_memory_data);
  }

  BeginDiskCacheAccessTimeCount();
  return entry_->GetEntry()->WriteData(kResponseInfoIndex, 0, data.get(),
                                       io_buf_len_, io_callback_, true);
}

int HttpCache::Transaction::OnWriteResponseInfoToEntryComplete(int result) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"),
                      "OnWriteResponseInfoToEntryComplete",
                      track_for_state_change_, "result", result);
  EndDiskCacheAccessTimeCount(DiskCacheAccessType::kWrite);
  if (!entry_) {
    return OK;
  }
  net_log_.EndEventWithNetErrorCode(NetLogEventType::HTTP_CACHE_WRITE_INFO,
                                    result);

  if (result != io_buf_len_) {
    DLOG(ERROR) << "failed to write response info to cache";
    DoneWithEntry(false);
  }
  return OK;
}

bool HttpCache::Transaction::StopCachingImpl(bool success) {
  bool stopped = false;
  // Let writers know so that it doesn't attempt to write to the cache.
  if (InWriters()) {
    stopped = entry_->writers()->StopCaching(success /* keep_entry */);
    if (stopped) {
      mode_ = NONE;
    }
  } else if (entry_) {
    stopped = true;
    DoneWithEntry(success /* entry_is_complete */);
  }
  return stopped;
}

void HttpCache::Transaction::DoneWithEntry(bool entry_is_complete) {
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("net"), "DoneWithEntry",
                      track_for_state_change_, "entry_is_complete",
                      entry_is_complete);
  if (!entry_) {
    return;
  }

  // Our `entry_` member must be valid throughout this call since
  // `DoneWithEntry` calls into
  // `HttpCache::Transaction::WriterAboutToBeRemovedFromEntry` which accesses
  // `this`'s `entry_` member.
  cache_->DoneWithEntry(entry_, this, entry_is_complete, partial_ != nullptr);
  entry_.reset();
  mode_ = NONE;  // switch to 'pass through' mode
}

int HttpCache::Transaction::OnCacheReadError(int result, bool restart) {
  DLOG(ERROR) << "ReadData failed: " << result;

  // Avoid using this entry in the future.
  if (cache_.get()) {
    cache_->DoomActiveEntry(cache_key_);
  }

  if (restart) {
    DCHECK(!reading_);
    DCHECK(!network_trans_.get());

    // Since we are going to add this to a new entry, not recording histograms
    // or setting mode to NONE at this point by invoking the wrapper
    // DoneWithEntry.
    //
    // Our `entry_` member must be valid throughout this call since
    // `DoneWithEntry` calls into
    // `HttpCache::Transaction::WriterAboutToBeRemovedFromEntry` which accesses
    // `this`'s `entry_` member.
    cache_->DoneWithEntry(entry_, this, true /* entry_is_complete */,
                          partial_ != nullptr);
    entry_.reset();
    is_sparse_ = false;
    // It's OK to use PartialData::RestoreHeaders here as |restart| is only set
    // when the HttpResponseInfo couldn't even be read, at which point it's
    // too early for range info in |partial_| to have changed.
    if (partial_) {
      partial_->RestoreHeaders(&mutable_request_->extra_headers);
    }
    partial_.reset();
    TransitionToState(STATE_GET_BACKEND);
    return OK;
  }

  TransitionToState(STATE_NONE);
  return ERR_CACHE_READ_FAILURE;
}

void HttpCache::Transaction::OnCacheLockTimeout(base::TimeTicks start_time) {
  if (entry_lock_waiting_since_ != start_time) {
    return;
  }

  DCHECK(next_state_ == STATE_ADD_TO_ENTRY_COMPLETE ||
         next_state_ == STATE_FINISH_HEADERS_COMPLETE || waiting_for_cache_io_);

  if (!cache_) {
    return;
  }

  if (next_state_ == STATE_ADD_TO_ENTRY_COMPLETE || waiting_for_cache_io_) {
    cache_->RemovePendingTransaction(this);
  } else {
    DoneWithEntry(false /* entry_is_complete */);
  }
  OnCacheIOComplete(ERR_CACHE_LOCK_TIMEOUT);
}

void HttpCache::Transaction::DoomPartialEntry(bool delete_object) {
  DVLOG(2) << "DoomPartialEntry";
  if (entry_ && !entry_->IsDoomed()) {
    int rv = cache_->DoomEntry(cache_key_, nullptr);
    DCHECK_EQ(OK, rv);
  }

  // Our `entry_` member must be valid throughout this call since
  // `DoneWithEntry` calls into
  // `HttpCache::Transaction::WriterAboutToBeRemovedFromEntry` which accesses
  // `this`'s `entry_` member.
  cache_->DoneWithEntry(entry_, this, false /* entry_is_complete */,
                        partial_ != nullptr);
  entry_.reset();
  is_sparse_ = false;
  truncated_ = false;
  if (delete_object) {
    partial_.reset(nullptr);
  }
}

int HttpCache::Transaction::DoPartialCacheReadCompleted(int result) {
  partial_->OnCacheReadCompleted(result);

  if (result == 0 && mode_ == READ_WRITE) {
    // We need to move on to the next range.
    TransitionToState(STATE_START_PARTIAL_CACHE_VALIDATION);
  } else if (result < 0) {
    return OnCacheReadError(result, false);
  } else {
    TransitionToState(STATE_NONE);
  }
  return result;
}

int HttpCache::Transaction::DoRestartPartialRequest() {
  // The stored data cannot be used. Get rid of it and restart this request.
  net_log_.AddEvent(NetLogEventType::HTTP_CACHE_RESTART_PARTIAL_REQUEST);

  // WRITE + Doom + STATE_INIT_ENTRY == STATE_CREATE_ENTRY (without an attempt
  // to Doom the entry again).
  ResetPartialState(!range_requested_);

  CHECK(!IsUsingURLFromNoVarySearchCache());

  // Change mode to WRITE after ResetPartialState as that may have changed the
  // mode to NONE.
  mode_ = WRITE;
  TransitionToState(STATE_CREATE_ENTRY);
  return OK;
}

void HttpCache::Transaction::ResetPartialState(bool delete_object) {
  partial_->RestoreHeaders(&mutable_request_->extra_headers);
  DoomPartialEntry(delete_object);

  if (!delete_object) {
    // The simplest way to re-initialize partial_ is to create a new object.
    partial_ = std::make_unique<PartialData>();

    // Reset the range header to the original value (http://crbug.com/820599).
    mutable_request_->extra_headers.RemoveHeader(HttpRequestHeaders::kRange);
    if (partial_->Init(initial_request_->extra_headers)) {
      partial_->SetHeaders(mutable_request_->extra_headers);
    } else {
      partial_.reset();
    }
  }
}

void HttpCache::Transaction::ResetNetworkTransaction() {
  SaveNetworkTransactionInfo(*network_trans_);
  network_trans_.reset();
}

const HttpTransaction* HttpCache::Transaction::network_transaction() const {
  if (network_trans_) {
    return network_trans_.get();
  }
  if (InWriters()) {
    return entry_->writers()->network_transaction();
  }
  return nullptr;
}

const HttpTransaction*
HttpCache::Transaction::GetOwnedOrMovedNetworkTransaction() const {
  if (network_trans_) {
    return network_trans_.get();
  }
  if (InWriters() && moved_network_transaction_to_writers_) {
    return entry_->writers()->network_transaction();
  }
  return nullptr;
}

HttpTransaction* HttpCache::Transaction::network_transaction() {
  return const_cast<HttpTransaction*>(
      static_cast<const Transaction*>(this)->network_transaction());
}

// Histogram data from the end of 2010 show the following distribution of
// response headers:
//
//   Content-Length............... 87%
//   Date......................... 98%
//   Last-Modified................ 49%
//   Etag......................... 19%
//   Accept-Ranges: bytes......... 25%
//   Accept-Ranges: none.......... 0.4%
//   Strong Validator............. 50%
//   Strong Validator + ranges.... 24%
//   Strong Validator + CL........ 49%
//
bool HttpCache::Transaction::CanResume(bool has_data) {
  // Double check that there is something worth keeping.
  if (has_data && !entry_->GetEntry()->GetDataSize(kResponseContentIndex)) {
    return false;
  }

  if (method_ != "GET") {
    return false;
  }

  // Note that if this is a 206, content-length was already fixed after calling
  // PartialData::ResponseHeadersOK().
  std::optional<base::ByteCount> content_length =
      response_.headers->GetContentLength();
  if (!content_length.has_value() || content_length->is_zero() ||
      response_.headers->HasHeaderValue("Accept-Ranges", "none") ||
      !response_.headers->HasStrongValidators()) {
    return false;
  }

  return true;
}

void HttpCache::Transaction::SetResponse(const HttpResponseInfo& response) {
  response_ = response;

  if (response_.headers) {
    DCHECK(request_);
    response_.vary_data.Init(*request_, *response_.headers);
  }

  SyncCacheEntryStatusToResponse();
}

void HttpCache::Transaction::SetAuthResponse(
    const HttpResponseInfo& auth_response) {
  auth_response_ = auth_response;
  SyncCacheEntryStatusToResponse();
}

void HttpCache::Transaction::UpdateCacheEntryStatus(
    CacheEntryStatus new_cache_entry_status) {
  DCHECK_NE(CacheEntryStatus::ENTRY_UNDEFINED, new_cache_entry_status);
  if (cache_entry_status_ == CacheEntryStatus::ENTRY_OTHER) {
    return;
  }
  DCHECK(cache_entry_status_ == CacheEntryStatus::ENTRY_UNDEFINED ||
         new_cache_entry_status == CacheEntryStatus::ENTRY_OTHER)
      << "cache_entry_status_: " << cache_entry_status_
      << "new_cache_entry_status: " << new_cache_entry_status;
  cache_entry_status_ = new_cache_entry_status;
  SyncCacheEntryStatusToResponse();
}

void HttpCache::Transaction::UpdateCacheEntryStatusToOther(
    OtherStatusReason reason) {
  if (cache_entry_status_ == CacheEntryStatus::ENTRY_OTHER) {
    return;
  }
  other_status_reason_ = reason;
  UpdateCacheEntryStatus(CacheEntryStatus::ENTRY_OTHER);
}

void HttpCache::Transaction::SyncCacheEntryStatusToResponse() {
  if (cache_entry_status_ == CacheEntryStatus::ENTRY_UNDEFINED) {
    return;
  }
  response_.cache_entry_status = cache_entry_status_;
  if (auth_response_.headers.get()) {
    auth_response_.cache_entry_status = cache_entry_status_;
  }
}

void HttpCache::Transaction::RecordHistograms() {
  DCHECK(!recorded_histograms_);
  recorded_histograms_ = true;

  if (CacheEntryStatus::ENTRY_UNDEFINED == cache_entry_status_) {
    return;
  }

  if (!cache_.get() || !cache_->GetCurrentBackend() ||
      cache_->GetCurrentBackend()->GetCacheType() != DISK_CACHE ||
      cache_->mode() != NORMAL || method_ != "GET") {
    return;
  }

  bool is_third_party = false;

  // Given that cache_entry_status_ is not ENTRY_UNDEFINED, the request must
  // have started and so request_ should exist.
  DCHECK(request_);
  if (request_->possibly_top_frame_origin) {
    is_third_party =
        !request_->possibly_top_frame_origin->IsSameOriginWith(request_->url);
  }

  std::string mime_type;
  HttpResponseHeaders* response_headers = GetResponseInfo()->headers.get();
  const bool is_no_store = response_headers && response_headers->HasHeaderValue(
                                                   "cache-control", "no-store");
  bool is_html = false;
  const bool is_main_frame = effective_load_flags_ & LOAD_MAIN_FRAME_DEPRECATED;
  if (response_headers && response_headers->GetMimeType(&mime_type)) {
    // Record the cache pattern by resource type. The type is inferred by
    // response header mime type, which could be incorrect, so this is just an
    // estimate.
    is_html = (mime_type == "text/html");
    if (is_html && is_main_frame) {
      CACHE_STATUS_HISTOGRAMS(".MainFrameHTML");
      IS_NO_STORE_HISTOGRAMS(".MainFrameHTML", is_no_store);
    } else if (is_html) {
      CACHE_STATUS_HISTOGRAMS(".NonMainFrameHTML");
    } else if (mime_type == "text/css") {
      if (is_third_party) {
        CACHE_STATUS_HISTOGRAMS(".CSSThirdParty");
      }
      CACHE_STATUS_HISTOGRAMS(".CSS");
    } else if (mime_type.starts_with("image/")) {
      std::optional<base::ByteCount> content_length =
          response_headers->GetContentLength();
      if (content_length) {
        if (content_length->InBytes() >= 0 && content_length->InBytes() < 100) {
          CACHE_STATUS_HISTOGRAMS(".TinyImage");
        } else if (content_length->InBytes() >= 100) {
          CACHE_STATUS_HISTOGRAMS(".NonTinyImage");
        }
      }
      CACHE_STATUS_HISTOGRAMS(".Image");
    } else if (mime_type.ends_with("javascript") ||
               mime_type.ends_with("ecmascript")) {
      if (is_third_party) {
        CACHE_STATUS_HISTOGRAMS(".JavaScriptThirdParty");
      }
      CACHE_STATUS_HISTOGRAMS(".JavaScript");
    } else if (mime_type.find("font") != std::string::npos) {
      if (is_third_party) {
        CACHE_STATUS_HISTOGRAMS(".FontThirdParty");
      }
      CACHE_STATUS_HISTOGRAMS(".Font");
    } else if (mime_type.starts_with("audio/")) {
      CACHE_STATUS_HISTOGRAMS(".Audio");
    } else if (mime_type.starts_with("video/")) {
      CACHE_STATUS_HISTOGRAMS(".Video");
    }
  }

  CACHE_STATUS_HISTOGRAMS("");
  IS_NO_STORE_HISTOGRAMS("", is_no_store);

  const bool did_send_request = !send_request_since_.is_null();

  if (no_vary_search_use_result_ == NoVarySearchUseResult::kUsed &&
      did_send_request) {
    no_vary_search_use_result_ =
        cache_entry_status_ == CacheEntryStatus::ENTRY_VALIDATED
            ? NoVarySearchUseResult::kValidated
            : NoVarySearchUseResult::kUpdated;
  }

  UMA_HISTOGRAM_ENUMERATION("HttpCache.NoVarySearch.UseResult2",
                            no_vary_search_use_result_);
  if (is_html && is_main_frame &&
      IsGoogleHostWithAlpnH3(request_->url.host())) {
    base::UmaHistogramEnumeration(
        "HttpCache.NoVarySearch.UseResult2.GoogleHost.MainFrameHTML",
        no_vary_search_use_result_);
  }

  if (cache_entry_status_ == CacheEntryStatus::ENTRY_OTHER) {
    CHECK_NE(other_status_reason_, OtherStatusReason::kNoReason);
    UMA_HISTOGRAM_ENUMERATION("HttpCache.Pattern.NotCoveredReason",
                              other_status_reason_);
    if (is_html && is_main_frame) {
      base::UmaHistogramEnumeration(
          "HttpCache.Pattern.NotCoveredReason.MainFrameHTML",
          other_status_reason_);
    }

    return;
  }

  DCHECK(!range_requested_) << "Cache entry status " << cache_entry_status_;
  DCHECK(!first_cache_access_since_.is_null());

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta total_time = now - first_cache_access_since_;

  UMA_HISTOGRAM_CUSTOM_TIMES("HttpCache.AccessToDone2", total_time,
                             base::Milliseconds(1), base::Seconds(30), 100);

  // It's not clear why `did_send_request` can be true when status is
  // ENTRY_USED. See https://crbug.com/1409150.
  // TODO(ricea): Maybe remove ENTRY_USED from the `did_send_request` true
  // branch once that issue is resolved.
  DCHECK(
      (did_send_request &&
       (cache_entry_status_ == CacheEntryStatus::ENTRY_NOT_IN_CACHE ||
        cache_entry_status_ == CacheEntryStatus::ENTRY_VALIDATED ||
        cache_entry_status_ == CacheEntryStatus::ENTRY_UPDATED ||
        cache_entry_status_ == CacheEntryStatus::ENTRY_CANT_CONDITIONALIZE ||
        cache_entry_status_ == CacheEntryStatus::ENTRY_USED)) ||
      (!did_send_request &&
       (cache_entry_status_ == CacheEntryStatus::ENTRY_USED ||
        cache_entry_status_ == CacheEntryStatus::ENTRY_CANT_CONDITIONALIZE)));


  if (!did_send_request) {
    if (cache_entry_status_ == CacheEntryStatus::ENTRY_USED) {
      UMA_HISTOGRAM_CUSTOM_TIMES("HttpCache.AccessToDone2.Used", total_time,
                                 base::Milliseconds(1), base::Seconds(3), 100);
    }
    return;
  }

  base::TimeDelta before_send_time =
      send_request_since_ - first_cache_access_since_;

  UMA_HISTOGRAM_CUSTOM_TIMES("HttpCache.AccessToDone2.SentRequest", total_time,
                             base::Milliseconds(1), base::Seconds(30), 100);
  UMA_HISTOGRAM_TIMES("HttpCache.BeforeSend", before_send_time);

  // TODO(gavinp): Remove or minimize these histograms, particularly the ones
  // below this comment after we have received initial data.
  switch (cache_entry_status_) {
    case CacheEntryStatus::ENTRY_CANT_CONDITIONALIZE: {
      UMA_HISTOGRAM_TIMES("HttpCache.BeforeSend.CantConditionalize",
                          before_send_time);
      break;
    }
    case CacheEntryStatus::ENTRY_NOT_IN_CACHE: {
      UMA_HISTOGRAM_TIMES("HttpCache.BeforeSend.NotCached", before_send_time);
      break;
    }
    case CacheEntryStatus::ENTRY_VALIDATED: {
      UMA_HISTOGRAM_TIMES("HttpCache.BeforeSend.Validated", before_send_time);
      break;
    }
    case CacheEntryStatus::ENTRY_UPDATED: {
      UMA_HISTOGRAM_TIMES("HttpCache.BeforeSend.Updated", before_send_time);
      break;
    }
    default:
      // STATUS_UNDEFINED and STATUS_OTHER are explicitly handled earlier in
      // the function so shouldn't reach here. STATUS_MAX should never be set.
      // Originally it was asserted that STATUS_USED couldn't happen here, but
      // it turns out that it can. We don't have histograms for it, so just
      // ignore it.
      DCHECK_EQ(cache_entry_status_, CacheEntryStatus::ENTRY_USED);
      break;
  }

  if (!total_disk_cache_read_time_.is_zero()) {
    base::UmaHistogramTimes("HttpCache.TotalDiskCacheTimePerTransaction.Read",
                            total_disk_cache_read_time_);
  }
  if (!total_disk_cache_write_time_.is_zero()) {
    base::UmaHistogramTimes("HttpCache.TotalDiskCacheTimePerTransaction.Write",
                            total_disk_cache_write_time_);
  }
}

bool HttpCache::Transaction::InWriters() const {
  return entry_ && entry_->HasWriters() &&
         entry_->writers()->HasTransaction(this);
}

HttpCache::Transaction::NetworkTransactionInfo::NetworkTransactionInfo() =
    default;
HttpCache::Transaction::NetworkTransactionInfo::~NetworkTransactionInfo() =
    default;

void HttpCache::Transaction::SaveNetworkTransactionInfo(
    const HttpTransaction& transaction) {
  DCHECK(!network_transaction_info_.old_network_trans_load_timing);
  LoadTimingInfo load_timing;
  if (transaction.GetLoadTimingInfo(&load_timing)) {
    network_transaction_info_.old_network_trans_load_timing =
        std::make_unique<LoadTimingInfo>(load_timing);
  }

  network_transaction_info_.total_received_bytes +=
      transaction.GetTotalReceivedBytes();
  network_transaction_info_.total_sent_bytes += transaction.GetTotalSentBytes();
  network_transaction_info_.received_body_bytes =
      transaction.GetReceivedBodyBytes();

  const auto connection_attempts = transaction.GetConnectionAttempts();
  network_transaction_info_.old_connection_attempts.insert(
      network_transaction_info_.old_connection_attempts.end(),
      connection_attempts.begin(), connection_attempts.end());
  network_transaction_info_.old_remote_endpoint = IPEndPoint();
  transaction.GetRemoteEndpoint(&network_transaction_info_.old_remote_endpoint);
}

void HttpCache::Transaction::OnIOComplete(int result) {
  if (waiting_for_cache_io_) {
    CHECK_NE(result, ERR_CACHE_RACE);
    // If the HttpCache IO hasn't completed yet, queue the IO result
    // to be processed when the HttpCache IO completes (or times out).
    pending_io_result_ = result;
  } else {
    DoLoop(result);
  }
}

void HttpCache::Transaction::OnCacheIOComplete(int result) {
  if (waiting_for_cache_io_) {
    // Handle the case of parallel HttpCache transactions being run against
    // network IO.
    waiting_for_cache_io_ = false;
    cache_pending_ = false;
    entry_lock_waiting_since_ = TimeTicks();

    if (result == OK) {
      entry_ = std::move(new_entry_);
    } else {
      // The HttpCache transaction failed or timed out. Bypass the cache in
      // this case independent of the state of the network IO callback.
      mode_ = NONE;
    }
    new_entry_.reset();

    // See if there is a pending IO result that completed while the HttpCache
    // transaction was being processed that now needs to be processed.
    if (pending_io_result_) {
      int stored_result = pending_io_result_.value();
      pending_io_result_ = std::nullopt;
      OnIOComplete(stored_result);
    }
  } else {
    DoLoop(result);
  }
}

void HttpCache::Transaction::TransitionToState(State state) {
  // Ensure that the state is only set once per Do* state.
  DCHECK(in_do_loop_);
  DCHECK_EQ(STATE_UNSET, next_state_) << "Next state is " << state;
  next_state_ = state;
}

bool HttpCache::Transaction::UpdateAndReportCacheability(
    const HttpResponseHeaders& headers) {
  // Do not cache no-store content.
  if (headers.HasHeaderValue("cache-control", "no-store")) {
    if (base::FeatureList::IsEnabled(features::kAvoidEntryCreationForNoStore)) {
      cache_->MarkKeyNoStore(cache_key_);
    }
    return true;
  }

  return false;
}

void HttpCache::Transaction::UpdateSecurityHeadersBeforeForwarding() {
  // Because of COEP, we need to add CORP to the 304 of resources that set it
  // previously. It will be blocked in the network service otherwise.
  std::string stored_corp_header =
      response_.headers->GetNormalizedHeader("Cross-Origin-Resource-Policy")
          .value_or(std::string());
  if (!stored_corp_header.empty()) {
    new_response_->headers->SetHeader("Cross-Origin-Resource-Policy",
                                      stored_corp_header);
  }
  return;
}

void HttpCache::Transaction::BeginDiskCacheAccessTimeCount() {
  DCHECK(last_disk_cache_access_start_time_.is_null());
  if (partial_) {
    return;
  }
  last_disk_cache_access_start_time_ = TimeTicks::Now();
}

void HttpCache::Transaction::EndDiskCacheAccessTimeCount(
    DiskCacheAccessType type) {
  // We may call this function without actual disk cache access as a result of
  // state change.
  if (last_disk_cache_access_start_time_.is_null()) {
    return;
  }
  base::TimeDelta elapsed =
      TimeTicks::Now() - last_disk_cache_access_start_time_;
  switch (type) {
    case DiskCacheAccessType::kRead:
      total_disk_cache_read_time_ += elapsed;
      break;
    case DiskCacheAccessType::kWrite:
      total_disk_cache_write_time_ += elapsed;
      break;
  }
  last_disk_cache_access_start_time_ = TimeTicks();
}

void HttpCache::Transaction::RecordEntrySizeHistograms(
    const disk_cache::Entry& entry) {
  // The comment in Simple Cache's EntryMetadata::SetEntrySize specifies the max
  // size of entries as 1/8th of the cache. There are multiple cache backends
  // and some active experiments around cache sizes, so this could mean
  // different max entry sizes in practice. The main goal of these histograms
  // being to evaluate the prevalence of "large" vs "small" entries, and a rough
  // estimate of entry size distributions, capping at 1/8th of the size under
  // the latest `ChangeDiskCacheSize` experiment is reasonable.
  static constexpr size_t kMaxEntrySize{160 * 1024 * 1024};

  auto header_size = entry.GetDataSize(kResponseInfoIndex);
  auto content_size = entry.GetDataSize(kResponseContentIndex);

  // UMA histograms starting at 1 will automatically get a [0, 1[ underflow
  // bucket. Use histogram macros instead of histogram functions since these
  // will be recorded often.
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "HttpCache.Experimental.Read.EntryResponseInfoSize", header_size, 1,
      kMaxEntrySize, 100);
  UMA_HISTOGRAM_CUSTOM_COUNTS("HttpCache.Experimental.Read.EntryContentSize",
                              content_size, 1, kMaxEntrySize, 100);
  UMA_HISTOGRAM_CUSTOM_COUNTS("HttpCache.Experimental.Read.EntryTotalSize",
                              header_size + content_size, 1, kMaxEntrySize,
                              100);
}

bool HttpCache::Transaction::IsNoVarySearchApplicable() const {
  return !partial_ && MethodUsesNoVarySearch(method_);
}

bool HttpCache::Transaction::IsUsingURLFromNoVarySearchCache() const {
  return no_vary_search_cache_erase_handle_.has_value();
}

HttpCache::Transaction::NoVarySearchUseResult
HttpCache::Transaction::LookupRequestInNoVarySearchCache() {
  // In order to conditionally log HttpCache.NoVarySearch.LookupTime.{Hit,Miss},
  // this doesn't use the SCOPED_UMA_HISTOGRAM_TIMER_MICROS macro, but the
  // bucket definitions are identical.
  bool base_url_matched = false;
  const auto start_time = base::Time::Now();
  std::optional<NoVarySearchCache::LookupResult> maybe_result =
      cache_->no_vary_search_cache_->Lookup(*request_, base_url_matched);
  const auto elapsed = base::Time::Now() - start_time;

  const bool is_main_frame = effective_load_flags_ & LOAD_MAIN_FRAME_DEPRECATED;
  // There are 12 similar histograms, so use macros to minimise copy-and-paste
  // errors.

#define UMA_HISTOGRAM_LOOKUP_TIME_SINGLE(full_suffix)           \
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(                      \
      "HttpCache.NoVarySearch.LookupTime" full_suffix, elapsed, \
      base::Microseconds(1), base::Seconds(1), 50)

#define UMA_HISTOGRAM_LOOKUP_TIME_CHECKING_BASE_URL_MATCH(suffix) \
  if (base_url_matched) {                                         \
    UMA_HISTOGRAM_LOOKUP_TIME_SINGLE(suffix ".BaseUrlMatched");   \
  }                                                               \
  UMA_HISTOGRAM_LOOKUP_TIME_SINGLE(suffix)

#define UMA_HISTOGRAM_LOOKUP_TIME(suffix)                                   \
  if (is_main_frame) {                                                      \
    UMA_HISTOGRAM_LOOKUP_TIME_CHECKING_BASE_URL_MATCH(suffix ".MainFrame"); \
  }                                                                         \
  UMA_HISTOGRAM_LOOKUP_TIME_CHECKING_BASE_URL_MATCH(suffix)

  UMA_HISTOGRAM_LOOKUP_TIME("");

  if (!maybe_result) {
    UMA_HISTOGRAM_LOOKUP_TIME(".Miss");
    return NoVarySearchUseResult::kNoMatch;
  }
  UMA_HISTOGRAM_LOOKUP_TIME(".Hit");

#undef UMA_HISTOGRAM_LOOKUP_TIME
#undef UMA_HISTOGRAM_LOOKUP_TIME_CHECKING_BASE_URL_MATCH
#undef UMA_HISTOGRAM_LOOKUP_TIME_SINGLE

  if (maybe_result->original_url == request_->url) {
    return NoVarySearchUseResult::kURLUnchanged;
  }
  NoVarySearchCache::LookupResult result = std::move(maybe_result).value();
  net_log_.BeginEvent(
      NetLogEventType::HTTP_CACHE_USING_NO_VARY_SEARCH_CACHE_URL, [&] {
        return base::Value::Dict()
            .Set("request_url", request_->url.spec())
            .Set("cached_url", result.original_url.spec());
      });
  EnsureMutableRequest();
  mutable_request_->url = std::move(result.original_url);
  no_vary_search_cache_erase_handle_ = std::move(result.erase_handle);

  // Regenerate the cache key with the modified URL.
  std::optional<std::string> cache_key =
      HttpCache::GenerateCacheKeyForRequest(request_);
  // NoVarySearchCache should never rewrite a URL that has a valid cache key to
  // one that doesn't.
  CHECK(cache_key);
  cache_key_ = cache_key.value();

  // May be updated to a different value later.
  return NoVarySearchUseResult::kUsed;
}

int HttpCache::Transaction::RestartWithoutNoVarySearchCache(
    RestartCacheEntryAction entry_action,
    NoVarySearchUseResult restart_reason) {
  no_vary_search_use_result_ = restart_reason;
  net_log_.EndEvent(
      NetLogEventType::HTTP_CACHE_USING_NO_VARY_SEARCH_CACHE_URL, [&] {
        return base::Value::Dict().Set(
            "restart_reason", NoVarySearchUseResultToString(restart_reason));
      });
  if (entry_action == RestartCacheEntryAction::kErase) {
    cache_->no_vary_search_cache_->Erase(
        std::move(no_vary_search_cache_erase_handle_.value()));
  }
  no_vary_search_cache_erase_handle_ = std::nullopt;
  // Don't try to use the NoVarySearchCache next time around.
  read_no_vary_search_cache_ = false;

  // If we've locked `entry_` we need to unlock it and permit other transactions
  // to proceed. `entry_is_complete` is true because we haven't modified the
  // entry.
  DoneWithEntry(/*entry_is_complete=*/true);

  // This will reset this object and send us back to the beginning of the
  // state machine to try again without using the NoVarySearchCache.
  TransitionToState(STATE_HEADERS_PHASE_CANNOT_PROCEED);
  return OK;
}

// static
std::string_view HttpCache::Transaction::NoVarySearchUseResultToString(
    NoVarySearchUseResult result) {
  using enum NoVarySearchUseResult;
  switch (result) {
    case kNotApplied:
      return "NotApplied";
    case kNoMatch:
      return "NoMatch";
    case kURLUnchanged:
      return "URLUnchanged";
    case kUsed:
      return "Used";
    case kNotSuitable:
      return "NotSuitable";
    case kNotOpenable:
      return "NotOpenable";
    case kReadOnlyNeedsValidation:
      return "ReadOnlyNeedsValidation";
    case kIncompleteBody:
      return "IncompleteBody";
    case kCouldntConditionalize:
      return "CouldntConditionalize";
    case kValidated:
      return "Validated";
    case kUpdated:
      return "Updated";
    case kCacheLockTimeout:
      return "CacheLockTimeout";
  }
}

void HttpCache::Transaction::EnsureMutableRequest() {
  if (!mutable_request_) {
    mutable_request_ = std::make_unique<HttpRequestInfo>(*request_);
    request_ = mutable_request_.get();
  }
}

}  // namespace net
