// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/341324165): Fix and remove.
#pragma allow_unsafe_buffers
#endif

#include "net/http/http_cache.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/sha1.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "http_request_info.h"
#include "net/base/cache_type.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_isolation_key.h"
#include "net/base/upload_data_stream.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache_transaction.h"
#include "net/http/http_cache_writers.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_server_info.h"
#include "url/origin.h"

#if BUILDFLAG(IS_POSIX)
#include <unistd.h>
#endif

namespace net {

namespace {
// True if any HTTP cache has been initialized.
bool g_init_cache = false;

// True if split cache is enabled by default. Must be set before any HTTP cache
// has been initialized.
bool g_enable_split_cache = false;

}  // namespace

const char HttpCache::kDoubleKeyPrefix[] = "_dk_";
const char HttpCache::kDoubleKeySeparator[] = " ";
const char HttpCache::kSubframeDocumentResourcePrefix[] = "s_";

HttpCache::DefaultBackend::DefaultBackend(
    CacheType type,
    BackendType backend_type,
    scoped_refptr<disk_cache::BackendFileOperationsFactory>
        file_operations_factory,
    const base::FilePath& path,
    int max_bytes,
    bool hard_reset)
    : type_(type),
      backend_type_(backend_type),
      file_operations_factory_(std::move(file_operations_factory)),
      path_(path),
      max_bytes_(max_bytes),
      hard_reset_(hard_reset) {}

HttpCache::DefaultBackend::~DefaultBackend() = default;

// static
std::unique_ptr<HttpCache::BackendFactory> HttpCache::DefaultBackend::InMemory(
    int max_bytes) {
  return std::make_unique<DefaultBackend>(MEMORY_CACHE, CACHE_BACKEND_DEFAULT,
                                          /*file_operations_factory=*/nullptr,
                                          base::FilePath(), max_bytes, false);
}

disk_cache::BackendResult HttpCache::DefaultBackend::CreateBackend(
    NetLog* net_log,
    base::OnceCallback<void(disk_cache::BackendResult)> callback) {
  DCHECK_GE(max_bytes_, 0);
  disk_cache::ResetHandling reset_handling =
      hard_reset_ ? disk_cache::ResetHandling::kReset
                  : disk_cache::ResetHandling::kResetOnError;
  LOCAL_HISTOGRAM_BOOLEAN("HttpCache.HardReset", hard_reset_);
#if BUILDFLAG(IS_ANDROID)
  if (app_status_listener_getter_) {
    return disk_cache::CreateCacheBackend(
        type_, backend_type_, file_operations_factory_, path_, max_bytes_,
        reset_handling, net_log, std::move(callback),
        app_status_listener_getter_);
  }
#endif
  return disk_cache::CreateCacheBackend(
      type_, backend_type_, file_operations_factory_, path_, max_bytes_,
      reset_handling, net_log, std::move(callback));
}

#if BUILDFLAG(IS_ANDROID)
void HttpCache::DefaultBackend::SetAppStatusListenerGetter(
    disk_cache::ApplicationStatusListenerGetter app_status_listener_getter) {
  app_status_listener_getter_ = std::move(app_status_listener_getter);
}
#endif

//-----------------------------------------------------------------------------

HttpCache::ActiveEntry::ActiveEntry(base::WeakPtr<HttpCache> cache,
                                    disk_cache::Entry* entry,
                                    bool opened_in)
    : cache_(std::move(cache)), disk_entry_(entry), opened_(opened_in) {
  CHECK(disk_entry_);
  cache_->active_entries_.emplace(disk_entry_->GetKey(),
                                  base::raw_ref<ActiveEntry>::from_ptr(this));
}

HttpCache::ActiveEntry::~ActiveEntry() {
  if (cache_) {
    if (doomed_) {
      FinalizeDoomed();
    } else {
      Deactivate();
    }
  }
}

void HttpCache::ActiveEntry::FinalizeDoomed() {
  CHECK(doomed_);

  auto it =
      cache_->doomed_entries_.find(base::raw_ref<ActiveEntry>::from_ptr(this));
  CHECK(it != cache_->doomed_entries_.end());

  cache_->doomed_entries_.erase(it);
}

void HttpCache::ActiveEntry::Deactivate() {
  CHECK(!doomed_);

  std::string key = disk_entry_->GetKey();
  if (key.empty()) {
    SlowDeactivate();
    return;
  }

  auto it = cache_->active_entries_.find(key);
  CHECK(it != cache_->active_entries_.end());
  CHECK(&it->second.get() == this);

  cache_->active_entries_.erase(it);
}

// TODO(ricea): Add unit test for this method.
void HttpCache::ActiveEntry::SlowDeactivate() {
  CHECK(cache_);
  // We don't know this entry's key so we have to find it without it.
  for (auto it = cache_->active_entries_.begin();
       it != cache_->active_entries_.end(); ++it) {
    if (&it->second.get() == this) {
      cache_->active_entries_.erase(it);
      return;
    }
  }
}

bool HttpCache::ActiveEntry::TransactionInReaders(
    Transaction* transaction) const {
  return readers_.count(transaction) > 0;
}

void HttpCache::ActiveEntry::ReleaseWriters() {
  // May destroy `this`.
  writers_.reset();
}

void HttpCache::ActiveEntry::AddTransactionToWriters(
    Transaction* transaction,
    ParallelWritingPattern parallel_writing_pattern) {
  CHECK(cache_);
  if (!writers_) {
    writers_ =
        std::make_unique<Writers>(cache_.get(), base::WrapRefCounted(this));
  } else {
    ParallelWritingPattern writers_pattern;
    DCHECK(writers_->CanAddWriters(&writers_pattern));
    DCHECK_EQ(PARALLEL_WRITING_JOIN, writers_pattern);
  }

  Writers::TransactionInfo info(transaction->partial(),
                                transaction->is_truncated(),
                                *(transaction->GetResponseInfo()));

  writers_->AddTransaction(transaction, parallel_writing_pattern,
                           transaction->priority(), info);
}

void HttpCache::ActiveEntry::Doom() {
  doomed_ = true;
  disk_entry_->Doom();
}

void HttpCache::ActiveEntry::RestartHeadersPhaseTransactions() {
  if (headers_transaction_) {
    RestartHeadersTransaction();
  }

  auto it = done_headers_queue_.begin();
  while (it != done_headers_queue_.end()) {
    Transaction* done_headers_transaction = *it;
    it = done_headers_queue_.erase(it);
    done_headers_transaction->cache_io_callback().Run(ERR_CACHE_RACE);
  }
}

void HttpCache::ActiveEntry::RestartHeadersTransaction() {
  Transaction* headers_transaction = headers_transaction_;
  headers_transaction_ = nullptr;
  // May destroy `this`.
  headers_transaction->SetValidatingCannotProceed();
}

void HttpCache::ActiveEntry::ProcessAddToEntryQueue() {
  DCHECK(!add_to_entry_queue_.empty());

  // Note `this` may be new or may already have a response body written to it.
  // In both cases, a transaction needs to wait since only one transaction can
  // be in the headers phase at a time.
  if (headers_transaction_) {
    return;
  }
  Transaction* transaction = add_to_entry_queue_.front();
  add_to_entry_queue_.erase(add_to_entry_queue_.begin());
  headers_transaction_ = transaction;

  transaction->cache_io_callback().Run(OK);
}

bool HttpCache::ActiveEntry::RemovePendingTransaction(
    Transaction* transaction) {
  auto j =
      find(add_to_entry_queue_.begin(), add_to_entry_queue_.end(), transaction);
  if (j == add_to_entry_queue_.end()) {
    return false;
  }

  add_to_entry_queue_.erase(j);
  return true;
}

HttpCache::TransactionList HttpCache::ActiveEntry::TakeAllQueuedTransactions() {
  // Process done_headers_queue before add_to_entry_queue to maintain FIFO
  // order.
  TransactionList list = std::move(done_headers_queue_);
  done_headers_queue_.clear();
  list.splice(list.end(), add_to_entry_queue_);
  add_to_entry_queue_.clear();
  return list;
}

bool HttpCache::ActiveEntry::CanTransactionWriteResponseHeaders(
    Transaction* transaction,
    bool is_partial,
    bool is_match) const {
  // If |transaction| is the current writer, do nothing. This can happen for
  // range requests since they can go back to headers phase after starting to
  // write.
  if (writers_ && writers_->HasTransaction(transaction)) {
    CHECK(is_partial);
    return true;
  }

  if (transaction != headers_transaction_) {
    return false;
  }

  if (!(transaction->mode() & Transaction::WRITE)) {
    return false;
  }

  // If its not a match then check if it is the transaction responsible for
  // writing the response body.
  if (!is_match) {
    return (!writers_ || writers_->IsEmpty()) && done_headers_queue_.empty() &&
           readers_.empty();
  }

  return true;
}

//-----------------------------------------------------------------------------

// This structure keeps track of work items that are attempting to create or
// open cache entries or the backend itself.
struct HttpCache::PendingOp {
  PendingOp() = default;
  ~PendingOp() = default;

  raw_ptr<disk_cache::Entry, AcrossTasksDanglingUntriaged> entry = nullptr;
  bool entry_opened = false;  // rather than created.

  std::unique_ptr<disk_cache::Backend> backend;
  std::unique_ptr<WorkItem> writer;
  // True if there is a posted OnPendingOpComplete() task that might delete
  // |this| without removing it from |pending_ops_|.  Note that since
  // OnPendingOpComplete() is static, it will not get cancelled when HttpCache
  // is destroyed.
  bool callback_will_delete = false;
  WorkItemList pending_queue;
};

//-----------------------------------------------------------------------------

// A work item encapsulates a single request to the backend with all the
// information needed to complete that request.
class HttpCache::WorkItem {
 public:
  WorkItem(WorkItemOperation operation,
           Transaction* transaction,
           scoped_refptr<ActiveEntry>* entry)
      : operation_(operation), transaction_(transaction), entry_(entry) {}
  WorkItem(WorkItemOperation operation,
           Transaction* transaction,
           CompletionOnceCallback callback)
      : operation_(operation),
        transaction_(transaction),
        entry_(nullptr),
        callback_(std::move(callback)) {}
  ~WorkItem() = default;

  // Calls back the transaction with the result of the operation.
  void NotifyTransaction(int result, scoped_refptr<ActiveEntry> entry) {
    if (entry_) {
      *entry_ = std::move(entry);
    }
    if (transaction_) {
      transaction_->cache_io_callback().Run(result);
    }
  }

  // Notifies the caller about the operation completion. Returns true if the
  // callback was invoked.
  bool DoCallback(int result) {
    if (!callback_.is_null()) {
      std::move(callback_).Run(result);
      return true;
    }
    return false;
  }

  WorkItemOperation operation() { return operation_; }
  void ClearTransaction() { transaction_ = nullptr; }
  void ClearEntry() { entry_ = nullptr; }
  void ClearCallback() { callback_.Reset(); }
  bool Matches(Transaction* transaction) const {
    return transaction == transaction_;
  }
  bool IsValid() const {
    return transaction_ || entry_ || !callback_.is_null();
  }

 private:
  WorkItemOperation operation_;
  raw_ptr<Transaction, DanglingUntriaged> transaction_;
  raw_ptr<scoped_refptr<ActiveEntry>, DanglingUntriaged> entry_;
  CompletionOnceCallback callback_;  // User callback.
};

//-----------------------------------------------------------------------------

HttpCache::HttpCache(std::unique_ptr<HttpTransactionFactory> network_layer,
                     std::unique_ptr<BackendFactory> backend_factory)
    : net_log_(nullptr),
      backend_factory_(std::move(backend_factory)),

      network_layer_(std::move(network_layer)),
      clock_(base::DefaultClock::GetInstance()),
      keys_marked_no_store_(
          features::kAvoidEntryCreationForNoStoreCacheSize.Get()) {
  g_init_cache = true;
  HttpNetworkSession* session = network_layer_->GetSession();
  // Session may be NULL in unittests.
  // TODO(mmenke): Seems like tests could be changed to provide a session,
  // rather than having logic only used in unit tests here.
  if (!session) {
    return;
  }

  net_log_ = session->net_log();
}

HttpCache::~HttpCache() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Transactions should see an invalid cache after this point; otherwise they
  // could see an inconsistent object (half destroyed).
  weak_factory_.InvalidateWeakPtrs();

  active_entries_.clear();
  doomed_entries_.clear();

  // Before deleting pending_ops_, we have to make sure that the disk cache is
  // done with said operations, or it will attempt to use deleted data.
  disk_cache_.reset();

  for (auto& pending_it : pending_ops_) {
    // We are not notifying the transactions about the cache going away, even
    // though they are waiting for a callback that will never fire.
    PendingOp* pending_op = pending_it.second;
    pending_op->writer.reset();
    bool delete_pending_op = true;
    if (building_backend_ && pending_op->callback_will_delete) {
      // If we don't have a backend, when its construction finishes it will
      // deliver the callbacks.
      delete_pending_op = false;
    }

    pending_op->pending_queue.clear();
    if (delete_pending_op) {
      delete pending_op;
    }
  }
}

HttpCache::GetBackendResult HttpCache::GetBackend(GetBackendCallback callback) {
  DCHECK(!callback.is_null());

  if (disk_cache_.get()) {
    return {OK, disk_cache_.get()};
  }

  int rv = CreateBackend(base::BindOnce(&HttpCache::ReportGetBackendResult,
                                        GetWeakPtr(), std::move(callback)));
  if (rv != ERR_IO_PENDING) {
    return {rv, disk_cache_.get()};
  }
  return {ERR_IO_PENDING, nullptr};
}

void HttpCache::ReportGetBackendResult(GetBackendCallback callback,
                                       int net_error) {
  std::move(callback).Run(std::pair(net_error, disk_cache_.get()));
}

disk_cache::Backend* HttpCache::GetCurrentBackend() const {
  return disk_cache_.get();
}

// static
bool HttpCache::ParseResponseInfo(base::span<const uint8_t> data,
                                  HttpResponseInfo* response_info,
                                  bool* response_truncated) {
  base::Pickle pickle = base::Pickle::WithUnownedBuffer(data);
  return response_info->InitFromPickle(pickle, response_truncated);
}

void HttpCache::CloseAllConnections(int net_error,
                                    const char* net_log_reason_utf8) {
  HttpNetworkSession* session = GetSession();
  if (session) {
    session->CloseAllConnections(net_error, net_log_reason_utf8);
  }
}

void HttpCache::CloseIdleConnections(const char* net_log_reason_utf8) {
  HttpNetworkSession* session = GetSession();
  if (session) {
    session->CloseIdleConnections(net_log_reason_utf8);
  }
}

void HttpCache::OnExternalCacheHit(
    const GURL& url,
    const std::string& http_method,
    const NetworkIsolationKey& network_isolation_key,
    bool used_credentials) {
  if (!disk_cache_.get() || mode_ == DISABLE) {
    return;
  }

  HttpRequestInfo request_info;
  request_info.url = url;
  request_info.method = http_method;
  request_info.network_isolation_key = network_isolation_key;
  request_info.network_anonymization_key =
      NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
          network_isolation_key);
  // This method is only called for cache hits on subresources, so mark this
  // request as not being a main frame or subframe navigation.
  request_info.is_subframe_document_resource = false;
  request_info.is_main_frame_navigation = false;
  request_info.initiator = std::nullopt;
  if (base::FeatureList::IsEnabled(features::kSplitCacheByIncludeCredentials)) {
    if (!used_credentials) {
      request_info.load_flags &= LOAD_DO_NOT_SAVE_COOKIES;
    } else {
      request_info.load_flags |= ~LOAD_DO_NOT_SAVE_COOKIES;
    }
  }

  std::optional<std::string> key = GenerateCacheKeyForRequest(&request_info);
  if (!key) {
    return;
  }
  disk_cache_->OnExternalCacheHit(*key);
}

int HttpCache::CreateTransaction(
    RequestPriority priority,
    std::unique_ptr<HttpTransaction>* transaction) {
  // Do lazy initialization of disk cache if needed.
  if (!disk_cache_.get()) {
    // We don't care about the result.
    CreateBackend(CompletionOnceCallback());
  }

  auto new_transaction =
      std::make_unique<HttpCache::Transaction>(priority, this);
  if (bypass_lock_for_test_) {
    new_transaction->BypassLockForTest();
  }
  if (bypass_lock_after_headers_for_test_) {
    new_transaction->BypassLockAfterHeadersForTest();
  }
  if (fail_conditionalization_for_test_) {
    new_transaction->FailConditionalizationForTest();
  }

  *transaction = std::move(new_transaction);
  return OK;
}

HttpCache* HttpCache::GetCache() {
  return this;
}

HttpNetworkSession* HttpCache::GetSession() {
  return network_layer_->GetSession();
}

std::unique_ptr<HttpTransactionFactory>
HttpCache::SetHttpNetworkTransactionFactoryForTesting(
    std::unique_ptr<HttpTransactionFactory> new_network_layer) {
  std::unique_ptr<HttpTransactionFactory> old_network_layer(
      std::move(network_layer_));
  network_layer_ = std::move(new_network_layer);
  return old_network_layer;
}

// static
std::string HttpCache::GetResourceURLFromHttpCacheKey(const std::string& key) {
  // The key format is:
  // credential_key/post_key/[isolation_key]url

  std::string::size_type pos = 0;
  pos = key.find('/', pos) + 1;  // Consume credential_key/
  pos = key.find('/', pos) + 1;  // Consume post_key/

  // It is a good idea to make this function tolerate invalid input. This can
  // happen because of disk corruption.
  if (pos == std::string::npos) {
    return "";
  }

  // Consume [isolation_key].
  // Search the key to see whether it begins with |kDoubleKeyPrefix|. If so,
  // then the entry was double-keyed.
  if (pos == key.find(kDoubleKeyPrefix, pos)) {
    // Find the rightmost occurrence of |kDoubleKeySeparator|, as when both
    // the top-frame origin and the initiator are added to the key, there will
    // be two occurrences of |kDoubleKeySeparator|.  When the cache entry is
    // originally written to disk, GenerateCacheKey method calls
    // HttpUtil::SpecForRequest method, which has a DCHECK to ensure that
    // the original resource url is valid, and hence will not contain the
    // unescaped whitespace of |kDoubleKeySeparator|.
    pos = key.rfind(kDoubleKeySeparator);
    DCHECK_NE(pos, std::string::npos);
    pos += strlen(kDoubleKeySeparator);
    DCHECK_LE(pos, key.size() - 1);
  }
  return key.substr(pos);
}

// static
bool HttpCache::CanGenerateCacheKeyForRequest(const HttpRequestInfo* request) {
  if (IsSplitCacheEnabled()) {
    if (request->network_isolation_key.IsTransient()) {
      return false;
    }
    // If the initiator is opaque, it would serialize to 'null' if used, which
    // would mean that navigations initiated from all opaque origins would share
    // a cache partition. To avoid this, we won't cache navigations where the
    // initiator is an opaque origin if the initiator would be used as part of
    // the cache key.
    if (request->initiator.has_value() && request->initiator->opaque()) {
      switch (HttpCache::GetExperimentMode()) {
        case HttpCache::ExperimentMode::kStandard:
        case HttpCache::ExperimentMode::kCrossSiteInitiatorBoolean:
          break;
        case HttpCache::ExperimentMode::kMainFrameNavigationInitiator:
          if (request->is_main_frame_navigation) {
            return false;
          }
          break;
        case HttpCache::ExperimentMode::kNavigationInitiator:
          if (request->is_main_frame_navigation ||
              request->is_subframe_document_resource) {
            return false;
          }
          break;
      }
    }
  }
  return true;
}

// static
// Generate a key that can be used inside the cache.
std::string HttpCache::GenerateCacheKey(
    const GURL& url,
    int load_flags,
    const NetworkIsolationKey& network_isolation_key,
    int64_t upload_data_identifier,
    bool is_subframe_document_resource,
    bool is_mainframe_navigation,
    std::optional<url::Origin> initiator) {
  // The first character of the key may vary depending on whether or not sending
  // credentials is permitted for this request. This only happens if the
  // SplitCacheByIncludeCredentials feature is enabled.
  const char credential_key = (base::FeatureList::IsEnabled(
                                   features::kSplitCacheByIncludeCredentials) &&
                               (load_flags & LOAD_DO_NOT_SAVE_COOKIES))
                                  ? '0'
                                  : '1';

  std::string isolation_key;
  if (IsSplitCacheEnabled()) {
    // Prepend the key with |kDoubleKeyPrefix| = "_dk_" to mark it as
    // double-keyed (and makes it an invalid url so that it doesn't get
    // confused with a single-keyed entry). Separate the origin and url
    // with invalid whitespace character |kDoubleKeySeparator|.
    CHECK(!network_isolation_key.IsTransient());

    const ExperimentMode experiment_mode = HttpCache::GetExperimentMode();
    std::string_view subframe_document_resource_prefix;
    if (is_subframe_document_resource) {
      switch (experiment_mode) {
        case HttpCache::ExperimentMode::kStandard:
        case HttpCache::ExperimentMode::kCrossSiteInitiatorBoolean:
        case HttpCache::ExperimentMode::kMainFrameNavigationInitiator:
          subframe_document_resource_prefix = kSubframeDocumentResourcePrefix;
          break;
        case HttpCache::ExperimentMode::kNavigationInitiator:
          // No need to set `subframe_document_resource_prefix` if we are
          // keying all cross-site navigations on initiator below.
          break;
      }
    }

    std::string navigation_experiment_prefix;
    if (initiator.has_value() &&
        (is_mainframe_navigation || is_subframe_document_resource)) {
      const auto initiator_site = net::SchemefulSite(*initiator);
      const bool is_initiator_cross_site =
          initiator_site != net::SchemefulSite(url);

      if (is_initiator_cross_site) {
        switch (experiment_mode) {
          case HttpCache::ExperimentMode::kStandard:
            break;
          case HttpCache::ExperimentMode::kCrossSiteInitiatorBoolean:
            if (is_mainframe_navigation) {
              navigation_experiment_prefix = "csnb_ ";
            }
            break;
          case HttpCache::ExperimentMode::kMainFrameNavigationInitiator:
            if (is_mainframe_navigation) {
              CHECK(!initiator_site.opaque());
              navigation_experiment_prefix =
                  base::StrCat({"mfni_", initiator_site.Serialize(), " "});
            }
            break;
          case HttpCache::ExperimentMode::kNavigationInitiator:
            if (is_mainframe_navigation || is_subframe_document_resource) {
              CHECK(!initiator_site.opaque());
              navigation_experiment_prefix =
                  base::StrCat({"ni_", initiator_site.Serialize(), " "});
            }
            break;
        }
      }
    }
    isolation_key = base::StrCat(
        {kDoubleKeyPrefix, subframe_document_resource_prefix,
         navigation_experiment_prefix,
         *network_isolation_key.ToCacheKeyString(), kDoubleKeySeparator});
  }

  // The key format is:
  // credential_key/upload_data_identifier/[isolation_key]url

  // Strip out the reference, username, and password sections of the URL and
  // concatenate with the credential_key, the post_key, and the network
  // isolation key if we are splitting the cache.
  return base::StringPrintf("%c/%" PRId64 "/%s%s", credential_key,
                            upload_data_identifier, isolation_key.c_str(),
                            HttpUtil::SpecForRequest(url).c_str());
}

// static
HttpCache::ExperimentMode HttpCache::GetExperimentMode() {
  bool cross_site_main_frame_navigation_boolean_enabled =
      base::FeatureList::IsEnabled(
          net::features::kSplitCacheByCrossSiteMainFrameNavigationBoolean);
  bool main_frame_navigation_initiator_enabled = base::FeatureList::IsEnabled(
      net::features::kSplitCacheByMainFrameNavigationInitiator);
  bool navigation_initiator_enabled = base::FeatureList::IsEnabled(
      net::features::kSplitCacheByNavigationInitiator);

  if (cross_site_main_frame_navigation_boolean_enabled) {
    if (main_frame_navigation_initiator_enabled ||
        navigation_initiator_enabled) {
      return ExperimentMode::kStandard;
    }
    return ExperimentMode::kCrossSiteInitiatorBoolean;
  } else if (main_frame_navigation_initiator_enabled) {
    if (navigation_initiator_enabled) {
      return ExperimentMode::kStandard;
    }
    return ExperimentMode::kMainFrameNavigationInitiator;
  } else if (navigation_initiator_enabled) {
    return ExperimentMode::kNavigationInitiator;
  }
  return ExperimentMode::kStandard;
}

// static
std::optional<std::string> HttpCache::GenerateCacheKeyForRequest(
    const HttpRequestInfo* request) {
  CHECK(request);

  if (!CanGenerateCacheKeyForRequest(request)) {
    return std::nullopt;
  }

  const int64_t upload_data_identifier =
      request->upload_data_stream ? request->upload_data_stream->identifier()
                                  : int64_t(0);
  return GenerateCacheKey(
      request->url, request->load_flags, request->network_isolation_key,
      upload_data_identifier, request->is_subframe_document_resource,
      request->is_main_frame_navigation, request->initiator);
}

// static
void HttpCache::SplitCacheFeatureEnableByDefault() {
  CHECK(!g_enable_split_cache && !g_init_cache);
  if (!base::FeatureList::GetInstance()->IsFeatureOverridden(
          "SplitCacheByNetworkIsolationKey")) {
    g_enable_split_cache = true;
  }
}

// static
bool HttpCache::IsSplitCacheEnabled() {
  return base::FeatureList::IsEnabled(
             features::kSplitCacheByNetworkIsolationKey) ||
         g_enable_split_cache;
}

// static
void HttpCache::ClearGlobalsForTesting() {
  // Reset these so that unit tests can work.
  g_init_cache = false;
  g_enable_split_cache = false;
}

//-----------------------------------------------------------------------------

Error HttpCache::CreateAndSetWorkItem(scoped_refptr<ActiveEntry>* entry,
                                      Transaction* transaction,
                                      WorkItemOperation operation,
                                      PendingOp* pending_op) {
  auto item = std::make_unique<WorkItem>(operation, transaction, entry);

  if (pending_op->writer) {
    pending_op->pending_queue.push_back(std::move(item));
    return ERR_IO_PENDING;
  }

  DCHECK(pending_op->pending_queue.empty());

  pending_op->writer = std::move(item);
  return OK;
}

int HttpCache::CreateBackend(CompletionOnceCallback callback) {
  DCHECK(!disk_cache_);

  if (!backend_factory_.get()) {
    return ERR_FAILED;
  }

  building_backend_ = true;

  const bool callback_is_null = callback.is_null();
  std::unique_ptr<WorkItem> item = std::make_unique<WorkItem>(
      WI_CREATE_BACKEND, nullptr, std::move(callback));

  // This is the only operation that we can do that is not related to any given
  // entry, so we use an empty key for it.
  PendingOp* pending_op = GetPendingOp(std::string());
  if (pending_op->writer) {
    if (!callback_is_null) {
      pending_op->pending_queue.push_back(std::move(item));
    }
    return ERR_IO_PENDING;
  }

  DCHECK(pending_op->pending_queue.empty());

  pending_op->writer = std::move(item);

  disk_cache::BackendResult result = backend_factory_->CreateBackend(
      net_log_, base::BindOnce(&HttpCache::OnPendingBackendCreationOpComplete,
                               GetWeakPtr(), pending_op));
  if (result.net_error == ERR_IO_PENDING) {
    pending_op->callback_will_delete = true;
    return result.net_error;
  }

  pending_op->writer->ClearCallback();
  int rv = result.net_error;
  OnPendingBackendCreationOpComplete(GetWeakPtr(), pending_op,
                                     std::move(result));
  return rv;
}

int HttpCache::GetBackendForTransaction(Transaction* transaction) {
  if (disk_cache_.get()) {
    return OK;
  }

  if (!building_backend_) {
    return ERR_FAILED;
  }

  std::unique_ptr<WorkItem> item = std::make_unique<WorkItem>(
      WI_CREATE_BACKEND, transaction, CompletionOnceCallback());
  PendingOp* pending_op = GetPendingOp(std::string());
  DCHECK(pending_op->writer);
  pending_op->pending_queue.push_back(std::move(item));
  return ERR_IO_PENDING;
}

void HttpCache::DoomActiveEntry(const std::string& key) {
  auto it = active_entries_.find(key);
  if (it == active_entries_.end()) {
    return;
  }

  // This is not a performance critical operation, this is handling an error
  // condition so it is OK to look up the entry again.
  int rv = DoomEntry(key, nullptr);
  DCHECK_EQ(OK, rv);
}

int HttpCache::DoomEntry(const std::string& key, Transaction* transaction) {
  // Need to abandon the ActiveEntry, but any transaction attached to the entry
  // should not be impacted.  Dooming an entry only means that it will no longer
  // be returned by GetActiveEntry (and it will also be destroyed once all
  // consumers are finished with the entry).
  auto it = active_entries_.find(key);
  if (it == active_entries_.end()) {
    DCHECK(transaction);
    return AsyncDoomEntry(key, transaction);
  }

  raw_ref<ActiveEntry> entry_ref = std::move(it->second);
  active_entries_.erase(it);

  // We keep track of doomed entries so that we can ensure that they are
  // cleaned up properly when the cache is destroyed.
  ActiveEntry& entry = entry_ref.get();
  DCHECK_EQ(0u, doomed_entries_.count(entry_ref));
  doomed_entries_.insert(std::move(entry_ref));

  entry.Doom();

  return OK;
}

int HttpCache::AsyncDoomEntry(const std::string& key,
                              Transaction* transaction) {
  PendingOp* pending_op = GetPendingOp(key);
  int rv =
      CreateAndSetWorkItem(nullptr, transaction, WI_DOOM_ENTRY, pending_op);
  if (rv != OK) {
    return rv;
  }

  RequestPriority priority = transaction ? transaction->priority() : LOWEST;
  rv = disk_cache_->DoomEntry(key, priority,
                              base::BindOnce(&HttpCache::OnPendingOpComplete,
                                             GetWeakPtr(), pending_op));
  if (rv == ERR_IO_PENDING) {
    pending_op->callback_will_delete = true;
    return rv;
  }

  pending_op->writer->ClearTransaction();
  OnPendingOpComplete(GetWeakPtr(), pending_op, rv);
  return rv;
}

void HttpCache::DoomMainEntryForUrl(
    const GURL& url,
    const NetworkIsolationKey& isolation_key,
    bool is_subframe_document_resource,
    bool is_main_frame_navigation,
    const std::optional<url::Origin>& initiator) {
  if (!disk_cache_) {
    return;
  }

  HttpRequestInfo temp_info;
  temp_info.url = url;
  temp_info.method = "GET";
  temp_info.network_isolation_key = isolation_key;
  temp_info.network_anonymization_key =
      NetworkAnonymizationKey::CreateFromNetworkIsolationKey(isolation_key);
  temp_info.is_subframe_document_resource = is_subframe_document_resource;
  temp_info.is_main_frame_navigation = is_main_frame_navigation;
  temp_info.initiator = initiator;

  std::optional<std::string> key = GenerateCacheKeyForRequest(&temp_info);
  if (!key) {
    return;
  }

  // Defer to DoomEntry if there is an active entry, otherwise call
  // AsyncDoomEntry without triggering a callback.
  if (active_entries_.count(*key)) {
    DoomEntry(*key, nullptr);
  } else {
    AsyncDoomEntry(*key, nullptr);
  }
}

bool HttpCache::HasActiveEntry(const std::string& key) {
  return active_entries_.find(key) != active_entries_.end();
}

scoped_refptr<HttpCache::ActiveEntry> HttpCache::GetActiveEntry(
    const std::string& key) {
  auto it = active_entries_.find(key);
  return it != active_entries_.end() ? base::WrapRefCounted(&it->second.get())
                                     : nullptr;
}

scoped_refptr<HttpCache::ActiveEntry> HttpCache::ActivateEntry(
    disk_cache::Entry* disk_entry,
    bool opened) {
  DCHECK(!HasActiveEntry(disk_entry->GetKey()));
  return base::MakeRefCounted<ActiveEntry>(weak_factory_.GetWeakPtr(),
                                           disk_entry, opened);
}

HttpCache::PendingOp* HttpCache::GetPendingOp(const std::string& key) {
  DCHECK(!HasActiveEntry(key));

  auto it = pending_ops_.find(key);
  if (it != pending_ops_.end()) {
    return it->second;
  }

  PendingOp* operation = new PendingOp();
  pending_ops_[key] = operation;
  return operation;
}

void HttpCache::DeletePendingOp(PendingOp* pending_op) {
  std::string key;
  if (pending_op->entry) {
    key = pending_op->entry->GetKey();
  }

  if (!key.empty()) {
    auto it = pending_ops_.find(key);
    CHECK(it != pending_ops_.end(), base::NotFatalUntil::M130);
    pending_ops_.erase(it);
  } else {
    for (auto it = pending_ops_.begin(); it != pending_ops_.end(); ++it) {
      if (it->second == pending_op) {
        pending_ops_.erase(it);
        break;
      }
    }
  }
  DCHECK(pending_op->pending_queue.empty());

  delete pending_op;
}

int HttpCache::OpenOrCreateEntry(const std::string& key,
                                 scoped_refptr<ActiveEntry>* entry,
                                 Transaction* transaction) {
  DCHECK(!HasActiveEntry(key));

  PendingOp* pending_op = GetPendingOp(key);
  int rv = CreateAndSetWorkItem(entry, transaction, WI_OPEN_OR_CREATE_ENTRY,
                                pending_op);
  if (rv != OK) {
    return rv;
  }

  disk_cache::EntryResult entry_result = disk_cache_->OpenOrCreateEntry(
      key, transaction->priority(),
      base::BindOnce(&HttpCache::OnPendingCreationOpComplete, GetWeakPtr(),
                     pending_op));
  rv = entry_result.net_error();
  if (rv == ERR_IO_PENDING) {
    pending_op->callback_will_delete = true;
    return ERR_IO_PENDING;
  }

  pending_op->writer->ClearTransaction();
  OnPendingCreationOpComplete(GetWeakPtr(), pending_op,
                              std::move(entry_result));
  return rv;
}

int HttpCache::OpenEntry(const std::string& key,
                         scoped_refptr<ActiveEntry>* entry,
                         Transaction* transaction) {
  DCHECK(!HasActiveEntry(key));

  PendingOp* pending_op = GetPendingOp(key);
  int rv = CreateAndSetWorkItem(entry, transaction, WI_OPEN_ENTRY, pending_op);
  if (rv != OK) {
    return rv;
  }

  disk_cache::EntryResult entry_result = disk_cache_->OpenEntry(
      key, transaction->priority(),
      base::BindOnce(&HttpCache::OnPendingCreationOpComplete, GetWeakPtr(),
                     pending_op));
  rv = entry_result.net_error();
  if (rv == ERR_IO_PENDING) {
    pending_op->callback_will_delete = true;
    return ERR_IO_PENDING;
  }

  pending_op->writer->ClearTransaction();
  OnPendingCreationOpComplete(GetWeakPtr(), pending_op,
                              std::move(entry_result));
  return rv;
}

int HttpCache::CreateEntry(const std::string& key,
                           scoped_refptr<ActiveEntry>* entry,
                           Transaction* transaction) {
  if (HasActiveEntry(key)) {
    return ERR_CACHE_RACE;
  }

  PendingOp* pending_op = GetPendingOp(key);
  int rv =
      CreateAndSetWorkItem(entry, transaction, WI_CREATE_ENTRY, pending_op);
  if (rv != OK) {
    return rv;
  }

  disk_cache::EntryResult entry_result = disk_cache_->CreateEntry(
      key, transaction->priority(),
      base::BindOnce(&HttpCache::OnPendingCreationOpComplete, GetWeakPtr(),
                     pending_op));
  rv = entry_result.net_error();
  if (rv == ERR_IO_PENDING) {
    pending_op->callback_will_delete = true;
    return ERR_IO_PENDING;
  }

  pending_op->writer->ClearTransaction();
  OnPendingCreationOpComplete(GetWeakPtr(), pending_op,
                              std::move(entry_result));
  return rv;
}

int HttpCache::AddTransactionToEntry(scoped_refptr<ActiveEntry>& entry,
                                     Transaction* transaction) {
  DCHECK(entry);
  DCHECK(entry->GetEntry());
  // Always add a new transaction to the queue to maintain FIFO order.
  entry->add_to_entry_queue().push_back(transaction);
  // Don't process the transaction if the lock timeout handling is being tested.
  if (!bypass_lock_for_test_) {
    ProcessQueuedTransactions(entry);
  }
  return ERR_IO_PENDING;
}

int HttpCache::DoneWithResponseHeaders(scoped_refptr<ActiveEntry>& entry,
                                       Transaction* transaction,
                                       bool is_partial) {
  // If |transaction| is the current writer, do nothing. This can happen for
  // range requests since they can go back to headers phase after starting to
  // write.
  if (entry->HasWriters() && entry->writers()->HasTransaction(transaction)) {
    DCHECK(is_partial && entry->writers()->GetTransactionsCount() == 1);
    return OK;
  }

  DCHECK_EQ(entry->headers_transaction(), transaction);

  entry->ClearHeadersTransaction();

  // If transaction is responsible for writing the response body, then do not go
  // through done_headers_queue for performance benefit. (Also, in case of
  // writer transaction, the consumer sometimes depend on synchronous behaviour
  // e.g. while computing raw headers size. (crbug.com/711766))
  if ((transaction->mode() & Transaction::WRITE) && !entry->HasWriters() &&
      entry->readers().empty()) {
    entry->AddTransactionToWriters(
        transaction, CanTransactionJoinExistingWriters(transaction));
    ProcessQueuedTransactions(entry);
    return OK;
  }

  entry->done_headers_queue().push_back(transaction);
  ProcessQueuedTransactions(entry);
  return ERR_IO_PENDING;
}

void HttpCache::DoneWithEntry(scoped_refptr<ActiveEntry>& entry,
                              Transaction* transaction,
                              bool entry_is_complete,
                              bool is_partial) {
  bool is_mode_read_only = transaction->mode() == Transaction::READ;

  if (!entry_is_complete && !is_mode_read_only && is_partial) {
    entry->GetEntry()->CancelSparseIO();
  }

  // Transaction is waiting in the done_headers_queue.
  auto it = base::ranges::find(entry->done_headers_queue(), transaction);
  if (it != entry->done_headers_queue().end()) {
    entry->done_headers_queue().erase(it);

    // Restart other transactions if this transaction could have written
    // response body.
    if (!entry_is_complete && !is_mode_read_only) {
      ProcessEntryFailure(entry.get());
    }
    return;
  }

  // Transaction is removed in the headers phase.
  if (transaction == entry->headers_transaction()) {
    entry->ClearHeadersTransaction();

    if (entry_is_complete || is_mode_read_only) {
      ProcessQueuedTransactions(entry);
    } else {
      // Restart other transactions if this transaction could have written
      // response body.
      ProcessEntryFailure(entry.get());
    }
    return;
  }

  // Transaction is removed in the writing phase.
  if (entry->HasWriters() && entry->writers()->HasTransaction(transaction)) {
    entry->writers()->RemoveTransaction(transaction,
                                        entry_is_complete /* success */);
    return;
  }

  // Transaction is reading from the entry.
  DCHECK(!entry->HasWriters());
  auto readers_it = entry->readers().find(transaction);
  CHECK(readers_it != entry->readers().end(), base::NotFatalUntil::M130);
  entry->readers().erase(readers_it);
  ProcessQueuedTransactions(entry);
}

void HttpCache::WritersDoomEntryRestartTransactions(ActiveEntry* entry) {
  DCHECK(!entry->writers()->IsEmpty());
  ProcessEntryFailure(entry);
}

void HttpCache::WritersDoneWritingToEntry(scoped_refptr<ActiveEntry> entry,
                                          bool success,
                                          bool should_keep_entry,
                                          TransactionSet make_readers) {
  // Impacts the queued transactions in one of the following ways:
  // - restart them but do not doom the entry since entry can be saved in
  // its truncated form.
  // - restart them and doom/destroy the entry since entry does not
  // have valid contents.
  // - let them continue by invoking their callback since entry is
  // successfully written.
  DCHECK(entry->HasWriters());
  DCHECK(entry->writers()->IsEmpty());
  DCHECK(success || make_readers.empty());

  if (!success && should_keep_entry) {
    // Restart already validated transactions so that they are able to read
    // the truncated status of the entry.
    entry->RestartHeadersPhaseTransactions();
    entry->ReleaseWriters();
    return;
  }

  if (success) {
    // Add any idle writers to readers.
    for (Transaction* reader : make_readers) {
      reader->WriteModeTransactionAboutToBecomeReader();
      entry->readers().insert(reader);
    }
    // Reset writers here so that WriteModeTransactionAboutToBecomeReader can
    // access the network transaction.
    entry->ReleaseWriters();
    ProcessQueuedTransactions(std::move(entry));
  } else {
    entry->ReleaseWriters();
    ProcessEntryFailure(entry.get());
  }
}

void HttpCache::DoomEntryValidationNoMatch(scoped_refptr<ActiveEntry> entry) {
  // Validating transaction received a non-matching response.
  DCHECK(entry->headers_transaction());

  entry->ClearHeadersTransaction();

  DoomActiveEntry(entry->GetEntry()->GetKey());

  // Restart only add_to_entry_queue transactions.
  // Post task here to avoid a race in creating the entry between |transaction|
  // and the add_to_entry_queue transactions. Reset the queued transaction's
  // cache pending state so that in case it's destructor is invoked, it's ok
  // for the transaction to not be found in this entry.
  for (HttpCache::Transaction* transaction : entry->add_to_entry_queue()) {
    transaction->ResetCachePendingState();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(transaction->cache_io_callback(), ERR_CACHE_RACE));
  }
  entry->add_to_entry_queue().clear();
}

void HttpCache::ProcessEntryFailure(ActiveEntry* entry) {
  // The writer failed to completely write the response to
  // the cache.

  if (entry->headers_transaction()) {
    entry->RestartHeadersTransaction();
  }

  TransactionList list = entry->TakeAllQueuedTransactions();

  DoomActiveEntry(entry->GetEntry()->GetKey());

  // ERR_CACHE_RACE causes the transaction to restart the whole process.
  for (Transaction* queued_transaction : list) {
    queued_transaction->cache_io_callback().Run(ERR_CACHE_RACE);
  }
}

void HttpCache::ProcessQueuedTransactions(scoped_refptr<ActiveEntry> entry) {
  // Multiple readers may finish with an entry at once, so we want to batch up
  // calls to OnProcessQueuedTransactions. This flag also tells us that we
  // should not delete the entry before OnProcessQueuedTransactions runs.
  if (entry->will_process_queued_transactions()) {
    return;
  }

  entry->set_will_process_queued_transactions(true);

  // Post a task instead of invoking the io callback of another transaction here
  // to avoid re-entrancy.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&HttpCache::OnProcessQueuedTransactions,
                                GetWeakPtr(), std::move(entry)));
}

void HttpCache::ProcessAddToEntryQueue(scoped_refptr<ActiveEntry> entry) {
  CHECK(!entry->add_to_entry_queue().empty());
  if (delay_add_transaction_to_entry_for_test_) {
    // Post a task to put the AddTransactionToEntry handling at the back of
    // the task queue. This allows other tasks (like network IO) to jump
    // ahead and simulate different callback ordering for testing.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&HttpCache::ProcessAddToEntryQueueImpl,
                                  GetWeakPtr(), std::move(entry)));
  } else {
    entry->ProcessAddToEntryQueue();
  }
}

void HttpCache::ProcessAddToEntryQueueImpl(scoped_refptr<ActiveEntry> entry) {
  entry->ProcessAddToEntryQueue();
}

HttpCache::ParallelWritingPattern HttpCache::CanTransactionJoinExistingWriters(
    Transaction* transaction) {
  if (transaction->method() != "GET") {
    return PARALLEL_WRITING_NOT_JOIN_METHOD_NOT_GET;
  }
  if (transaction->partial()) {
    return PARALLEL_WRITING_NOT_JOIN_RANGE;
  }
  if (transaction->mode() == Transaction::READ) {
    return PARALLEL_WRITING_NOT_JOIN_READ_ONLY;
  }
  if (transaction->GetResponseInfo()->headers &&
      transaction->GetResponseInfo()->headers->GetContentLength() >
          disk_cache_->MaxFileSize()) {
    return PARALLEL_WRITING_NOT_JOIN_TOO_BIG_FOR_CACHE;
  }
  return PARALLEL_WRITING_JOIN;
}

void HttpCache::ProcessDoneHeadersQueue(scoped_refptr<ActiveEntry> entry) {
  ParallelWritingPattern writers_pattern;
  DCHECK(!entry->HasWriters() ||
         entry->writers()->CanAddWriters(&writers_pattern));
  DCHECK(!entry->done_headers_queue().empty());

  Transaction* transaction = entry->done_headers_queue().front();

  ParallelWritingPattern parallel_writing_pattern =
      CanTransactionJoinExistingWriters(transaction);
  if (entry->IsWritingInProgress()) {
    if (parallel_writing_pattern != PARALLEL_WRITING_JOIN) {
      // TODO(shivanisha): Returning from here instead of checking the next
      // transaction in the queue because the FIFO order is maintained
      // throughout, until it becomes a reader or writer. May be at this point
      // the ordering is not important but that would be optimizing a rare
      // scenario where write mode transactions are insterspersed with read-only
      // transactions.
      return;
    }
    entry->AddTransactionToWriters(transaction, parallel_writing_pattern);
  } else {  // no writing in progress
    if (transaction->mode() & Transaction::WRITE) {
      if (transaction->partial()) {
        if (entry->readers().empty()) {
          entry->AddTransactionToWriters(transaction, parallel_writing_pattern);
        } else {
          return;
        }
      } else {
        // Add the transaction to readers since the response body should have
        // already been written. (If it was the first writer about to start
        // writing to the cache, it would have been added to writers in
        // DoneWithResponseHeaders, thus no writers here signify the response
        // was completely written).
        transaction->WriteModeTransactionAboutToBecomeReader();
        auto return_val = entry->readers().insert(transaction);
        DCHECK(return_val.second);
      }
    } else {  // mode READ
      auto return_val = entry->readers().insert(transaction);
      DCHECK(return_val.second);
    }
  }

  // Post another task to give a chance to more transactions to either join
  // readers or another transaction to start parallel validation.
  ProcessQueuedTransactions(entry);

  entry->done_headers_queue().erase(entry->done_headers_queue().begin());
  transaction->cache_io_callback().Run(OK);
}

LoadState HttpCache::GetLoadStateForPendingTransaction(
    const Transaction* transaction) {
  auto i = active_entries_.find(transaction->key());
  if (i == active_entries_.end()) {
    // If this is really a pending transaction, and it is not part of
    // active_entries_, we should be creating the backend or the entry.
    return LOAD_STATE_WAITING_FOR_CACHE;
  }

  Writers* writers = i->second->writers();
  return !writers ? LOAD_STATE_WAITING_FOR_CACHE : writers->GetLoadState();
}

void HttpCache::RemovePendingTransaction(Transaction* transaction) {
  auto i = active_entries_.find(transaction->key());
  bool found = false;
  if (i != active_entries_.end()) {
    found = i->second->RemovePendingTransaction(transaction);
  }

  if (found) {
    return;
  }

  if (building_backend_) {
    auto j = pending_ops_.find(std::string());
    if (j != pending_ops_.end()) {
      found = RemovePendingTransactionFromPendingOp(j->second, transaction);
    }

    if (found) {
      return;
    }
  }

  auto j = pending_ops_.find(transaction->key());
  if (j != pending_ops_.end()) {
    found = RemovePendingTransactionFromPendingOp(j->second, transaction);
  }

  if (found) {
    return;
  }

  for (auto k = doomed_entries_.begin(); k != doomed_entries_.end() && !found;
       ++k) {
    // TODO(ricea): Add unit test for this line.
    found = k->get().RemovePendingTransaction(transaction);
  }

  DCHECK(found) << "Pending transaction not found";
}

bool HttpCache::RemovePendingTransactionFromPendingOp(
    PendingOp* pending_op,
    Transaction* transaction) {
  if (pending_op->writer->Matches(transaction)) {
    pending_op->writer->ClearTransaction();
    pending_op->writer->ClearEntry();
    return true;
  }
  WorkItemList& pending_queue = pending_op->pending_queue;

  for (auto it = pending_queue.begin(); it != pending_queue.end(); ++it) {
    if ((*it)->Matches(transaction)) {
      pending_queue.erase(it);
      return true;
    }
  }
  return false;
}

void HttpCache::MarkKeyNoStore(const std::string& key) {
  keys_marked_no_store_.Put(base::SHA1Hash(base::as_byte_span(key)));
}

bool HttpCache::DidKeyLeadToNoStoreResponse(const std::string& key) {
  return keys_marked_no_store_.Get(base::SHA1Hash(base::as_byte_span(key))) !=
         keys_marked_no_store_.end();
}

void HttpCache::OnProcessQueuedTransactions(scoped_refptr<ActiveEntry> entry) {
  entry->set_will_process_queued_transactions(false);

  // Note that this function should only invoke one transaction's IO callback
  // since its possible for IO callbacks' consumers to destroy the cache/entry.

  if (entry->done_headers_queue().empty() &&
      entry->add_to_entry_queue().empty()) {
    return;
  }

  // To maintain FIFO order of transactions, done_headers_queue should be
  // checked for processing before add_to_entry_queue.

  // If another transaction is writing the response, let validated transactions
  // wait till the response is complete. If the response is not yet started, the
  // done_headers_queue transaction should start writing it.
  if (!entry->done_headers_queue().empty()) {
    ParallelWritingPattern unused_reason;
    if (!entry->writers() || entry->writers()->CanAddWriters(&unused_reason)) {
      ProcessDoneHeadersQueue(entry);
      return;
    }
  }

  if (!entry->add_to_entry_queue().empty()) {
    ProcessAddToEntryQueue(std::move(entry));
  }
}

void HttpCache::OnIOComplete(int result, PendingOp* pending_op) {
  WorkItemOperation op = pending_op->writer->operation();

  // Completing the creation of the backend is simpler than the other cases.
  if (op == WI_CREATE_BACKEND) {
    return OnBackendCreated(result, pending_op);
  }

  std::unique_ptr<WorkItem> item = std::move(pending_op->writer);
  bool try_restart_requests = false;

  scoped_refptr<ActiveEntry> entry;
  std::string key;
  if (result == OK) {
    if (op == WI_DOOM_ENTRY) {
      // Anything after a Doom has to be restarted.
      try_restart_requests = true;
    } else if (item->IsValid()) {
      DCHECK(pending_op->entry);
      key = pending_op->entry->GetKey();
      entry = ActivateEntry(pending_op->entry, pending_op->entry_opened);
    } else {
      // The writer transaction is gone.
      if (!pending_op->entry_opened) {
        pending_op->entry->Doom();
      }

      pending_op->entry->Close();
      pending_op->entry = nullptr;
      try_restart_requests = true;
    }
  }

  // We are about to notify a bunch of transactions, and they may decide to
  // re-issue a request (or send a different one). If we don't delete
  // pending_op, the new request will be appended to the end of the list, and
  // we'll see it again from this point before it has a chance to complete (and
  // we'll be messing out the request order). The down side is that if for some
  // reason notifying request A ends up cancelling request B (for the same key),
  // we won't find request B anywhere (because it would be in a local variable
  // here) and that's bad. If there is a chance for that to happen, we'll have
  // to move the callback used to be a CancelableOnceCallback. By the way, for
  // this to happen the action (to cancel B) has to be synchronous to the
  // notification for request A.
  WorkItemList pending_items = std::move(pending_op->pending_queue);
  DeletePendingOp(pending_op);

  item->NotifyTransaction(result, entry);

  while (!pending_items.empty()) {
    item = std::move(pending_items.front());
    pending_items.pop_front();

    if (item->operation() == WI_DOOM_ENTRY) {
      // A queued doom request is always a race.
      try_restart_requests = true;
    } else if (result == OK) {
      entry = GetActiveEntry(key);
      if (!entry) {
        try_restart_requests = true;
      }
    }

    if (try_restart_requests) {
      item->NotifyTransaction(ERR_CACHE_RACE, nullptr);
      continue;
    }
    // At this point item->operation() is anything except Doom.
    if (item->operation() == WI_CREATE_ENTRY) {
      if (result == OK) {
        // Successful OpenOrCreate, Open, or Create followed by a Create.
        item->NotifyTransaction(ERR_CACHE_CREATE_FAILURE, nullptr);
      } else {
        if (op != WI_CREATE_ENTRY && op != WI_OPEN_OR_CREATE_ENTRY) {
          // Failed Open or Doom followed by a Create.
          item->NotifyTransaction(ERR_CACHE_RACE, nullptr);
          try_restart_requests = true;
        } else {
          item->NotifyTransaction(result, entry);
        }
      }
    }
    // item->operation() is OpenOrCreate or Open
    else if (item->operation() == WI_OPEN_OR_CREATE_ENTRY) {
      if ((op == WI_OPEN_ENTRY || op == WI_CREATE_ENTRY) && result != OK) {
        // Failed Open or Create followed by an OpenOrCreate.
        item->NotifyTransaction(ERR_CACHE_RACE, nullptr);
        try_restart_requests = true;
      } else {
        item->NotifyTransaction(result, entry);
      }
    }
    // item->operation() is Open.
    else {
      if (op == WI_CREATE_ENTRY && result != OK) {
        // Failed Create followed by an Open.
        item->NotifyTransaction(ERR_CACHE_RACE, nullptr);
        try_restart_requests = true;
      } else {
        item->NotifyTransaction(result, entry);
      }
    }
  }
}

// static
void HttpCache::OnPendingOpComplete(base::WeakPtr<HttpCache> cache,
                                    PendingOp* pending_op,
                                    int rv) {
  if (cache.get()) {
    pending_op->callback_will_delete = false;
    cache->OnIOComplete(rv, pending_op);
  } else {
    // The callback was cancelled so we should delete the pending_op that
    // was used with this callback.
    delete pending_op;
  }
}

// static
void HttpCache::OnPendingCreationOpComplete(base::WeakPtr<HttpCache> cache,
                                            PendingOp* pending_op,
                                            disk_cache::EntryResult result) {
  if (!cache.get()) {
    // The callback was cancelled so we should delete the pending_op that
    // was used with this callback. If |result| contains a fresh entry
    // it will close it automatically, since we don't release it here.
    delete pending_op;
    return;
  }

  int rv = result.net_error();
  pending_op->entry_opened = result.opened();
  pending_op->entry = result.ReleaseEntry();
  pending_op->callback_will_delete = false;
  cache->OnIOComplete(rv, pending_op);
}

// static
void HttpCache::OnPendingBackendCreationOpComplete(
    base::WeakPtr<HttpCache> cache,
    PendingOp* pending_op,
    disk_cache::BackendResult result) {
  if (!cache.get()) {
    // The callback was cancelled so we should delete the pending_op that
    // was used with this callback. If `result` contains a cache backend,
    // it will be destroyed with it.
    delete pending_op;
    return;
  }

  int rv = result.net_error;
  pending_op->backend = std::move(result.backend);
  pending_op->callback_will_delete = false;
  cache->OnIOComplete(rv, pending_op);
}

void HttpCache::OnBackendCreated(int result, PendingOp* pending_op) {
  std::unique_ptr<WorkItem> item = std::move(pending_op->writer);
  WorkItemOperation op = item->operation();
  DCHECK_EQ(WI_CREATE_BACKEND, op);

  if (backend_factory_.get()) {
    // We may end up calling OnBackendCreated multiple times if we have pending
    // work items. The first call saves the backend and releases the factory,
    // and the last call clears building_backend_.
    backend_factory_.reset();  // Reclaim memory.
    if (result == OK) {
      disk_cache_ = std::move(pending_op->backend);
      UMA_HISTOGRAM_MEMORY_KB("HttpCache.MaxFileSizeOnInit",
                              disk_cache_->MaxFileSize() / 1024);
    }
  }

  if (!pending_op->pending_queue.empty()) {
    std::unique_ptr<WorkItem> pending_item =
        std::move(pending_op->pending_queue.front());
    pending_op->pending_queue.pop_front();
    DCHECK_EQ(WI_CREATE_BACKEND, pending_item->operation());

    // We want to process a single callback at a time, because the cache may
    // go away from the callback.
    pending_op->writer = std::move(pending_item);

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&HttpCache::OnBackendCreated, GetWeakPtr(),
                                  result, pending_op));
  } else {
    building_backend_ = false;
    DeletePendingOp(pending_op);
  }

  // The cache may be gone when we return from the callback.
  if (!item->DoCallback(result)) {
    item->NotifyTransaction(result, nullptr);
  }
}

}  // namespace net
