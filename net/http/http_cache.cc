// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_cache.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/pickle.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/process_memory_dump.h"
#include "net/base/cache_type.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/upload_data_stream.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache_lookup_manager.h"
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

#if defined(OS_POSIX)
#include <unistd.h>
#endif

namespace net {

const char HttpCache::kDoubleKeyPrefix[] = "_dk_";
const char HttpCache::kDoubleKeySeparator[] = " ";

HttpCache::DefaultBackend::DefaultBackend(CacheType type,
                                          BackendType backend_type,
                                          const base::FilePath& path,
                                          int max_bytes)
    : type_(type),
      backend_type_(backend_type),
      path_(path),
      max_bytes_(max_bytes) {}

HttpCache::DefaultBackend::~DefaultBackend() = default;

// static
std::unique_ptr<HttpCache::BackendFactory> HttpCache::DefaultBackend::InMemory(
    int max_bytes) {
  return std::make_unique<DefaultBackend>(MEMORY_CACHE, CACHE_BACKEND_DEFAULT,
                                          base::FilePath(), max_bytes);
}

int HttpCache::DefaultBackend::CreateBackend(
    NetLog* net_log,
    std::unique_ptr<disk_cache::Backend>* backend,
    CompletionOnceCallback callback) {
  DCHECK_GE(max_bytes_, 0);
  // TODO(crbug.com/1002220): Implement a forced reset for the http_cache when
  // the Finch experiment status changes the cache configuration.
#if defined(OS_ANDROID)
  if (app_status_listener_) {
    return disk_cache::CreateCacheBackend(
        type_, backend_type_, path_, max_bytes_,
        disk_cache::ResetHandling::kResetOnError, net_log, backend,
        std::move(callback), app_status_listener_);
  }
#endif
  return disk_cache::CreateCacheBackend(
      type_, backend_type_, path_, max_bytes_,
      disk_cache::ResetHandling::kResetOnError, net_log, backend,
      std::move(callback));
}

#if defined(OS_ANDROID)
void HttpCache::DefaultBackend::SetAppStatusListener(
    base::android::ApplicationStatusListener* app_status_listener) {
  app_status_listener_ = app_status_listener;
}
#endif

//-----------------------------------------------------------------------------

HttpCache::ActiveEntry::ActiveEntry(disk_cache::Entry* entry, bool opened_in)
    : disk_entry(entry), opened(opened_in) {}

HttpCache::ActiveEntry::~ActiveEntry() {
  if (disk_entry) {
    disk_entry->Close();
    disk_entry = nullptr;
  }
}

size_t HttpCache::ActiveEntry::EstimateMemoryUsage() const {
  // Skip |disk_entry| which is tracked in simple_backend_impl; Skip |readers|
  // and |add_to_entry_queue| because the Transactions are owned by their
  // respective URLRequestHttpJobs.
  return 0;
}

bool HttpCache::ActiveEntry::HasNoTransactions() {
  return (!writers || writers->IsEmpty()) && readers.empty() &&
         add_to_entry_queue.empty() && done_headers_queue.empty() &&
         !headers_transaction;
}

bool HttpCache::ActiveEntry::SafeToDestroy() {
  return HasNoTransactions() && !writers && !will_process_queued_transactions;
}

bool HttpCache::ActiveEntry::TransactionInReaders(
    Transaction* transaction) const {
  return readers.count(transaction) > 0;
}

//-----------------------------------------------------------------------------

// This structure keeps track of work items that are attempting to create or
// open cache entries or the backend itself.
struct HttpCache::PendingOp {
  PendingOp()
      : entry(nullptr), entry_opened(false), callback_will_delete(false) {}
  ~PendingOp() = default;

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const {
    // Note that backend isn't counted because it doesn't provide an EMU
    // function.
    return base::trace_event::EstimateMemoryUsage(writer) +
           base::trace_event::EstimateMemoryUsage(pending_queue);
  }

  disk_cache::Entry* entry;
  bool entry_opened;  // rather than created.

  std::unique_ptr<disk_cache::Backend> backend;
  std::unique_ptr<WorkItem> writer;
  // True if there is a posted OnPendingOpComplete() task that might delete
  // |this| without removing it from |pending_ops_|.  Note that since
  // OnPendingOpComplete() is static, it will not get cancelled when HttpCache
  // is destroyed.
  bool callback_will_delete;
  WorkItemList pending_queue;
};

//-----------------------------------------------------------------------------

// A work item encapsulates a single request to the backend with all the
// information needed to complete that request.
class HttpCache::WorkItem {
 public:
  WorkItem(WorkItemOperation operation,
           Transaction* transaction,
           ActiveEntry** entry)
      : operation_(operation),
        transaction_(transaction),
        entry_(entry),
        backend_(nullptr) {}
  WorkItem(WorkItemOperation operation,
           Transaction* transaction,
           CompletionOnceCallback callback,
           disk_cache::Backend** backend)
      : operation_(operation),
        transaction_(transaction),
        entry_(nullptr),
        callback_(std::move(callback)),
        backend_(backend) {}
  ~WorkItem() = default;

  // Calls back the transaction with the result of the operation.
  void NotifyTransaction(int result, ActiveEntry* entry) {
    DCHECK(!entry || entry->disk_entry);
    if (entry_)
      *entry_ = entry;
    if (transaction_)
      transaction_->io_callback().Run(result);
  }

  // Notifies the caller about the operation completion. Returns true if the
  // callback was invoked.
  bool DoCallback(int result, disk_cache::Backend* backend) {
    if (backend_)
      *backend_ = backend;
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

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const { return 0; }

 private:
  WorkItemOperation operation_;
  Transaction* transaction_;
  ActiveEntry** entry_;
  CompletionOnceCallback callback_;  // User callback.
  disk_cache::Backend** backend_;
};

//-----------------------------------------------------------------------------

HttpCache::HttpCache(HttpNetworkSession* session,
                     std::unique_ptr<BackendFactory> backend_factory,
                     bool is_main_cache)
    : HttpCache(std::make_unique<HttpNetworkLayer>(session),
                std::move(backend_factory),
                is_main_cache) {}

HttpCache::HttpCache(std::unique_ptr<HttpTransactionFactory> network_layer,
                     std::unique_ptr<BackendFactory> backend_factory,
                     bool is_main_cache)
    : net_log_(nullptr),
      backend_factory_(std::move(backend_factory)),
      building_backend_(false),
      bypass_lock_for_test_(false),
      bypass_lock_after_headers_for_test_(false),
      fail_conditionalization_for_test_(false),
      mode_(NORMAL),
      network_layer_(std::move(network_layer)),
      clock_(base::DefaultClock::GetInstance()) {
  HttpNetworkSession* session = network_layer_->GetSession();
  // Session may be NULL in unittests.
  // TODO(mmenke): Seems like tests could be changed to provide a session,
  // rather than having logic only used in unit tests here.
  if (!session)
    return;

  net_log_ = session->net_log();
  if (!is_main_cache)
    return;

  session->SetServerPushDelegate(
      std::make_unique<HttpCacheLookupManager>(this));
}

HttpCache::~HttpCache() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Transactions should see an invalid cache after this point; otherwise they
  // could see an inconsistent object (half destroyed).
  weak_factory_.InvalidateWeakPtrs();

  // If we have any active entries remaining, then we need to deactivate them.
  // We may have some pending tasks to process queued transactions ,but since
  // those won't run (due to our destruction), we can simply ignore the
  // corresponding flags.
  while (!active_entries_.empty()) {
    ActiveEntry* entry = active_entries_.begin()->second.get();
    entry->will_process_queued_transactions = false;
    entry->add_to_entry_queue.clear();
    entry->readers.clear();
    entry->done_headers_queue.clear();
    entry->headers_transaction = nullptr;
    entry->writers.reset();
    DeactivateEntry(entry);
  }

  doomed_entries_.clear();

  // Before deleting pending_ops_, we have to make sure that the disk cache is
  // done with said operations, or it will attempt to use deleted data.
  disk_cache_.reset();

  for (auto pending_it = pending_ops_.begin(); pending_it != pending_ops_.end();
       ++pending_it) {
    // We are not notifying the transactions about the cache going away, even
    // though they are waiting for a callback that will never fire.
    PendingOp* pending_op = pending_it->second;
    pending_op->writer.reset();
    bool delete_pending_op = true;
    if (building_backend_ && pending_op->callback_will_delete) {
      // If we don't have a backend, when its construction finishes it will
      // deliver the callbacks.
      delete_pending_op = false;
    }

    pending_op->pending_queue.clear();
    if (delete_pending_op)
      delete pending_op;
  }
}

int HttpCache::GetBackend(disk_cache::Backend** backend,
                          CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());

  if (disk_cache_.get()) {
    *backend = disk_cache_.get();
    return OK;
  }

  return CreateBackend(backend, std::move(callback));
}

disk_cache::Backend* HttpCache::GetCurrentBackend() const {
  return disk_cache_.get();
}

// static
bool HttpCache::ParseResponseInfo(const char* data, int len,
                                  HttpResponseInfo* response_info,
                                  bool* response_truncated) {
  base::Pickle pickle(data, len);
  return response_info->InitFromPickle(pickle, response_truncated);
}

void HttpCache::CloseAllConnections() {
  HttpNetworkSession* session = GetSession();
  if (session)
    session->CloseAllConnections();
}

void HttpCache::CloseIdleConnections() {
  HttpNetworkSession* session = GetSession();
  if (session)
    session->CloseIdleConnections();
}

void HttpCache::OnExternalCacheHit(
    const GURL& url,
    const std::string& http_method,
    const NetworkIsolationKey& network_isolation_key) {
  if (!disk_cache_.get() || mode_ == DISABLE)
    return;

  HttpRequestInfo request_info;
  request_info.url = url;
  request_info.method = http_method;
  request_info.network_isolation_key = network_isolation_key;
  std::string key = GenerateCacheKey(&request_info);
  disk_cache_->OnExternalCacheHit(key);
}

int HttpCache::CreateTransaction(
    RequestPriority priority,
    std::unique_ptr<HttpTransaction>* transaction) {
  // Do lazy initialization of disk cache if needed.
  if (!disk_cache_.get()) {
    // We don't care about the result.
    CreateBackend(nullptr, CompletionOnceCallback());
  }

  HttpCache::Transaction* new_transaction =
      new HttpCache::Transaction(priority, this);
  if (bypass_lock_for_test_)
    new_transaction->BypassLockForTest();
  if (bypass_lock_after_headers_for_test_)
    new_transaction->BypassLockAfterHeadersForTest();
  if (fail_conditionalization_for_test_)
    new_transaction->FailConditionalizationForTest();

  transaction->reset(new_transaction);
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

void HttpCache::DumpMemoryStats(base::trace_event::ProcessMemoryDump* pmd,
                                const std::string& parent_absolute_name) const {
  // Skip tracking members like |clock_| and |backend_factory_| because they
  // don't allocate.
  std::string name = parent_absolute_name + "/http_cache";
  base::trace_event::MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(name);
  size_t size = base::trace_event::EstimateMemoryUsage(active_entries_) +
                base::trace_event::EstimateMemoryUsage(doomed_entries_) +
                base::trace_event::EstimateMemoryUsage(playback_cache_map_) +
                base::trace_event::EstimateMemoryUsage(pending_ops_);
  if (disk_cache_)
    size += disk_cache_->DumpMemoryStats(pmd, name);

  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes, size);
}

std::string HttpCache::GetResourceURLFromHttpCacheKey(const std::string& key) {
  // Search the key to see whether it begins with |kDoubleKeyPrefix|. If so,
  // then the entry was double-keyed.
  if (base::StartsWith(key, kDoubleKeyPrefix, base::CompareCase::SENSITIVE)) {
    // Find the rightmost occurrence of |kDoubleKeySeparator|, as when both
    // the top-frame origin and the initiator are added to the key, there will
    // be two occurrences of |kDoubleKeySeparator|.  When the cache entry is
    // originally written to disk, GenerateCacheKey method calls
    // HttpUtil::SpecForRequest method, which has a DCHECK to ensure that
    // the original resource url is valid, and hence will not contain the
    // unescaped whitespace of |kDoubleKeySeparator|.
    size_t separator_position = key.rfind(kDoubleKeySeparator);
    DCHECK_NE(separator_position, std::string::npos);

    size_t separator_size = strlen(kDoubleKeySeparator);
    size_t start_position = separator_position + separator_size;
    DCHECK_LE(start_position, key.size() - 1);

    return key.substr(start_position);
  }
  return key;
}

// static
std::string HttpCache::GenerateCacheKeyForTest(const HttpRequestInfo* request) {
  return GenerateCacheKey(request);
}

//-----------------------------------------------------------------------------

net::Error HttpCache::CreateAndSetWorkItem(ActiveEntry** entry,
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

int HttpCache::CreateBackend(disk_cache::Backend** backend,
                             CompletionOnceCallback callback) {
  if (!backend_factory_.get())
    return ERR_FAILED;

  building_backend_ = true;

  const bool callback_is_null = callback.is_null();
  std::unique_ptr<WorkItem> item = std::make_unique<WorkItem>(
      WI_CREATE_BACKEND, nullptr, std::move(callback), backend);

  // This is the only operation that we can do that is not related to any given
  // entry, so we use an empty key for it.
  PendingOp* pending_op = GetPendingOp(std::string());
  if (pending_op->writer) {
    if (!callback_is_null)
      pending_op->pending_queue.push_back(std::move(item));
    return ERR_IO_PENDING;
  }

  DCHECK(pending_op->pending_queue.empty());

  pending_op->writer = std::move(item);

  int rv = backend_factory_->CreateBackend(
      net_log_, &pending_op->backend,
      base::BindOnce(&HttpCache::OnPendingOpComplete, GetWeakPtr(),
                     pending_op));
  if (rv == ERR_IO_PENDING) {
    pending_op->callback_will_delete = true;
    return rv;
  }

  pending_op->writer->ClearCallback();
  OnPendingOpComplete(GetWeakPtr(), pending_op, rv);
  return rv;
}

int HttpCache::GetBackendForTransaction(Transaction* transaction) {
  if (disk_cache_.get())
    return OK;

  if (!building_backend_)
    return ERR_FAILED;

  std::unique_ptr<WorkItem> item = std::make_unique<WorkItem>(
      WI_CREATE_BACKEND, transaction, CompletionOnceCallback(), nullptr);
  PendingOp* pending_op = GetPendingOp(std::string());
  DCHECK(pending_op->writer);
  pending_op->pending_queue.push_back(std::move(item));
  return ERR_IO_PENDING;
}

// static
// Generate a key that can be used inside the cache.
std::string HttpCache::GenerateCacheKey(const HttpRequestInfo* request) {
  std::string isolation_key;

  if (base::FeatureList::IsEnabled(
          features::kSplitCacheByNetworkIsolationKey)) {
    // Prepend the key with |kDoubleKeyPrefix| = "_dk_" to mark it as
    // double-keyed (and makes it an invalid url so that it doesn't get
    // confused with a single-keyed entry). Separate the origin and url
    // with invalid whitespace character |kDoubleKeySeparator|.
    DCHECK(request->network_isolation_key.IsFullyPopulated());
    isolation_key = base::StrCat({kDoubleKeyPrefix,
                                  request->network_isolation_key.ToString(),
                                  kDoubleKeySeparator});
  }

  // Strip out the reference, username, and password sections of the URL and
  // concatenate with the network isolation key if we are splitting the cache.
  std::string url = isolation_key + HttpUtil::SpecForRequest(request->url);

  // No valid URL can begin with numerals, so we should not have to worry
  // about collisions with normal URLs.
  if (request->upload_data_stream &&
      request->upload_data_stream->identifier()) {
    url.insert(0,
               base::StringPrintf("%" PRId64 "/",
                                  request->upload_data_stream->identifier()));
  }

  return url;
}

void HttpCache::DoomActiveEntry(const std::string& key) {
  auto it = active_entries_.find(key);
  if (it == active_entries_.end())
    return;

  // This is not a performance critical operation, this is handling an error
  // condition so it is OK to look up the entry again.
  int rv = DoomEntry(key, nullptr);
  DCHECK_EQ(OK, rv);
}

int HttpCache::DoomEntry(const std::string& key, Transaction* transaction) {
  // Need to abandon the ActiveEntry, but any transaction attached to the entry
  // should not be impacted.  Dooming an entry only means that it will no
  // longer be returned by FindActiveEntry (and it will also be destroyed once
  // all consumers are finished with the entry).
  auto it = active_entries_.find(key);
  if (it == active_entries_.end()) {
    DCHECK(transaction);
    return AsyncDoomEntry(key, transaction);
  }

  std::unique_ptr<ActiveEntry> entry = std::move(it->second);
  active_entries_.erase(it);

  // We keep track of doomed entries so that we can ensure that they are
  // cleaned up properly when the cache is destroyed.
  ActiveEntry* entry_ptr = entry.get();
  DCHECK_EQ(0u, doomed_entries_.count(entry_ptr));
  doomed_entries_[entry_ptr] = std::move(entry);

  entry_ptr->disk_entry->Doom();
  entry_ptr->doomed = true;

  DCHECK(!entry_ptr->SafeToDestroy());
  return OK;
}

int HttpCache::AsyncDoomEntry(const std::string& key,
                              Transaction* transaction) {
  PendingOp* pending_op = GetPendingOp(key);
  int rv =
      CreateAndSetWorkItem(nullptr, transaction, WI_DOOM_ENTRY, pending_op);
  if (rv != OK)
    return rv;

  net::RequestPriority priority =
      transaction ? transaction->priority() : net::LOWEST;
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

void HttpCache::DoomMainEntryForUrl(const GURL& url,
                                    const NetworkIsolationKey& isolation_key) {
  if (!disk_cache_)
    return;

  HttpRequestInfo temp_info;
  temp_info.url = url;
  temp_info.method = "GET";
  temp_info.network_isolation_key = isolation_key;
  std::string key = GenerateCacheKey(&temp_info);

  // Defer to DoomEntry if there is an active entry, otherwise call
  // AsyncDoomEntry without triggering a callback.
  if (active_entries_.count(key))
    DoomEntry(key, nullptr);
  else
    AsyncDoomEntry(key, nullptr);
}

void HttpCache::FinalizeDoomedEntry(ActiveEntry* entry) {
  DCHECK(entry->doomed);
  DCHECK(entry->SafeToDestroy());

  auto it = doomed_entries_.find(entry);
  DCHECK(it != doomed_entries_.end());
  doomed_entries_.erase(it);
}

HttpCache::ActiveEntry* HttpCache::FindActiveEntry(const std::string& key) {
  auto it = active_entries_.find(key);
  return it != active_entries_.end() ? it->second.get() : nullptr;
}

HttpCache::ActiveEntry* HttpCache::ActivateEntry(disk_cache::Entry* disk_entry,
                                                 bool opened) {
  DCHECK(!FindActiveEntry(disk_entry->GetKey()));
  ActiveEntry* entry = new ActiveEntry(disk_entry, opened);
  active_entries_[disk_entry->GetKey()] = base::WrapUnique(entry);
  return entry;
}

void HttpCache::DeactivateEntry(ActiveEntry* entry) {
  DCHECK(!entry->doomed);
  DCHECK(entry->disk_entry);
  DCHECK(entry->SafeToDestroy());

  std::string key = entry->disk_entry->GetKey();
  if (key.empty())
    return SlowDeactivateEntry(entry);

  auto it = active_entries_.find(key);
  DCHECK(it != active_entries_.end());
  DCHECK(it->second.get() == entry);

  active_entries_.erase(it);
}

// We don't know this entry's key so we have to find it without it.
void HttpCache::SlowDeactivateEntry(ActiveEntry* entry) {
  for (auto it = active_entries_.begin(); it != active_entries_.end(); ++it) {
    if (it->second.get() == entry) {
      active_entries_.erase(it);
      break;
    }
  }
}

HttpCache::PendingOp* HttpCache::GetPendingOp(const std::string& key) {
  DCHECK(!FindActiveEntry(key));

  auto it = pending_ops_.find(key);
  if (it != pending_ops_.end())
    return it->second;

  PendingOp* operation = new PendingOp();
  pending_ops_[key] = operation;
  return operation;
}

void HttpCache::DeletePendingOp(PendingOp* pending_op) {
  std::string key;
  if (pending_op->entry)
    key = pending_op->entry->GetKey();

  if (!key.empty()) {
    auto it = pending_ops_.find(key);
    DCHECK(it != pending_ops_.end());
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
                                 ActiveEntry** entry,
                                 Transaction* transaction) {
  DCHECK(!FindActiveEntry(key));

  PendingOp* pending_op = GetPendingOp(key);
  int rv = CreateAndSetWorkItem(entry, transaction, WI_OPEN_OR_CREATE_ENTRY,
                                pending_op);
  if (rv != OK)
    return rv;

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
                         ActiveEntry** entry,
                         Transaction* transaction) {
  DCHECK(!FindActiveEntry(key));

  PendingOp* pending_op = GetPendingOp(key);
  int rv = CreateAndSetWorkItem(entry, transaction, WI_OPEN_ENTRY, pending_op);
  if (rv != OK)
    return rv;

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
                           ActiveEntry** entry,
                           Transaction* transaction) {
  if (FindActiveEntry(key)) {
    return ERR_CACHE_RACE;
  }

  PendingOp* pending_op = GetPendingOp(key);
  int rv =
      CreateAndSetWorkItem(entry, transaction, WI_CREATE_ENTRY, pending_op);
  if (rv != OK)
    return rv;

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

void HttpCache::DestroyEntry(ActiveEntry* entry) {
  if (entry->doomed) {
    FinalizeDoomedEntry(entry);
  } else {
    DeactivateEntry(entry);
  }
}

int HttpCache::AddTransactionToEntry(ActiveEntry* entry,
                                     Transaction* transaction) {
  DCHECK(entry);
  DCHECK(entry->disk_entry);
  // Always add a new transaction to the queue to maintain FIFO order.
  entry->add_to_entry_queue.push_back(transaction);
  ProcessQueuedTransactions(entry);
  return ERR_IO_PENDING;
}

int HttpCache::DoneWithResponseHeaders(ActiveEntry* entry,
                                       Transaction* transaction,
                                       bool is_partial) {
  // If |transaction| is the current writer, do nothing. This can happen for
  // range requests since they can go back to headers phase after starting to
  // write.
  if (entry->writers && entry->writers->HasTransaction(transaction)) {
    DCHECK(is_partial && entry->writers->GetTransactionsCount() == 1);
    return OK;
  }

  DCHECK_EQ(entry->headers_transaction, transaction);

  entry->headers_transaction = nullptr;

  // If transaction is responsible for writing the response body, then do not go
  // through done_headers_queue for performance benefit. (Also, in case of
  // writer transaction, the consumer sometimes depend on synchronous behaviour
  // e.g. while computing raw headers size. (crbug.com/711766))
  if ((transaction->mode() & Transaction::WRITE) && !entry->writers &&
      entry->readers.empty()) {
    AddTransactionToWriters(entry, transaction,
                            CanTransactionJoinExistingWriters(transaction));
    ProcessQueuedTransactions(entry);
    return OK;
  }

  entry->done_headers_queue.push_back(transaction);
  ProcessQueuedTransactions(entry);
  return ERR_IO_PENDING;
}

void HttpCache::DoneWithEntry(ActiveEntry* entry,
                              Transaction* transaction,
                              bool entry_is_complete,
                              bool is_partial) {
  bool is_mode_read_only = transaction->mode() == Transaction::READ;

  if (!entry_is_complete && !is_mode_read_only && is_partial)
    entry->disk_entry->CancelSparseIO();

  // Transaction is waiting in the done_headers_queue.
  auto it = std::find(entry->done_headers_queue.begin(),
                      entry->done_headers_queue.end(), transaction);
  if (it != entry->done_headers_queue.end()) {
    entry->done_headers_queue.erase(it);

    // Restart other transactions if this transaction could have written
    // response body.
    if (!entry_is_complete && !is_mode_read_only)
      ProcessEntryFailure(entry);
    return;
  }

  // Transaction is removed in the headers phase.
  if (transaction == entry->headers_transaction) {
    entry->headers_transaction = nullptr;

    if (entry_is_complete || is_mode_read_only) {
      ProcessQueuedTransactions(entry);
    } else {
      // Restart other transactions if this transaction could have written
      // response body.
      ProcessEntryFailure(entry);
    }
    return;
  }

  // Transaction is removed in the writing phase.
  if (entry->writers && entry->writers->HasTransaction(transaction)) {
    entry->writers->RemoveTransaction(transaction,
                                      entry_is_complete /* success */);
    return;
  }

  // Transaction is reading from the entry.
  DCHECK(!entry->writers);
  auto readers_it = entry->readers.find(transaction);
  DCHECK(readers_it != entry->readers.end());
  entry->readers.erase(readers_it);
  ProcessQueuedTransactions(entry);
}

void HttpCache::WritersDoomEntryRestartTransactions(ActiveEntry* entry) {
  DCHECK(!entry->writers->IsEmpty());
  ProcessEntryFailure(entry);
}

void HttpCache::WritersDoneWritingToEntry(ActiveEntry* entry,
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
  DCHECK(entry->writers);
  DCHECK(entry->writers->IsEmpty());
  DCHECK(success || make_readers.empty());

  if (!success && should_keep_entry) {
    // Restart already validated transactions so that they are able to read
    // the truncated status of the entry.
    RestartHeadersPhaseTransactions(entry);
    entry->writers.reset();
    if (entry->SafeToDestroy()) {
      DestroyEntry(entry);
    }
    return;
  }

  if (success) {
    // Add any idle writers to readers.
    for (auto* reader : make_readers) {
      reader->WriteModeTransactionAboutToBecomeReader();
      entry->readers.insert(reader);
    }
    // Reset writers here so that WriteModeTransactionAboutToBecomeReader can
    // access the network transaction.
    entry->writers.reset();
    ProcessQueuedTransactions(entry);
  } else {
    entry->writers.reset();
    ProcessEntryFailure(entry);
  }
}

void HttpCache::DoomEntryValidationNoMatch(ActiveEntry* entry) {
  // Validating transaction received a non-matching response.
  DCHECK(entry->headers_transaction);

  entry->headers_transaction = nullptr;
  if (entry->SafeToDestroy()) {
    entry->disk_entry->Doom();
    DestroyEntry(entry);
    return;
  }

  DoomActiveEntry(entry->disk_entry->GetKey());

  // Restart only add_to_entry_queue transactions.
  // Post task here to avoid a race in creating the entry between |transaction|
  // and the add_to_entry_queue transactions. Reset the queued transaction's
  // cache pending state so that in case it's destructor is invoked, it's ok
  // for the transaction to not be found in this entry.
  for (auto* transaction : entry->add_to_entry_queue) {
    transaction->ResetCachePendingState();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(transaction->io_callback(), net::ERR_CACHE_RACE));
  }
  entry->add_to_entry_queue.clear();
}

void HttpCache::RemoveAllQueuedTransactions(ActiveEntry* entry,
                                            TransactionList* list) {
  // Process done_headers_queue before add_to_entry_queue to maintain FIFO
  // order.

  for (auto* transaction : entry->done_headers_queue)
    list->push_back(transaction);
  entry->done_headers_queue.clear();

  for (auto* pending_transaction : entry->add_to_entry_queue)
    list->push_back(pending_transaction);
  entry->add_to_entry_queue.clear();
}

void HttpCache::ProcessEntryFailure(ActiveEntry* entry) {
  // The writer failed to completely write the response to
  // the cache.

  if (entry->headers_transaction)
    RestartHeadersTransaction(entry);

  TransactionList list;
  RemoveAllQueuedTransactions(entry, &list);

  if (entry->SafeToDestroy()) {
    entry->disk_entry->Doom();
    DestroyEntry(entry);
  } else {
    DoomActiveEntry(entry->disk_entry->GetKey());
  }
  // ERR_CACHE_RACE causes the transaction to restart the whole process.
  for (auto* queued_transaction : list)
    queued_transaction->io_callback().Run(net::ERR_CACHE_RACE);
}

void HttpCache::RestartHeadersPhaseTransactions(ActiveEntry* entry) {
  if (entry->headers_transaction)
    RestartHeadersTransaction(entry);

  auto it = entry->done_headers_queue.begin();
  while (it != entry->done_headers_queue.end()) {
    Transaction* done_headers_transaction = *it;
    it = entry->done_headers_queue.erase(it);
    done_headers_transaction->io_callback().Run(net::ERR_CACHE_RACE);
  }
}

void HttpCache::RestartHeadersTransaction(ActiveEntry* entry) {
  entry->headers_transaction->SetValidatingCannotProceed();
  entry->headers_transaction = nullptr;
}

void HttpCache::ProcessQueuedTransactions(ActiveEntry* entry) {
  // Multiple readers may finish with an entry at once, so we want to batch up
  // calls to OnProcessQueuedTransactions. This flag also tells us that we
  // should not delete the entry before OnProcessQueuedTransactions runs.
  if (entry->will_process_queued_transactions)
    return;

  entry->will_process_queued_transactions = true;

  // Post a task instead of invoking the io callback of another transaction here
  // to avoid re-entrancy.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&HttpCache::OnProcessQueuedTransactions,
                                GetWeakPtr(), entry));
}

void HttpCache::ProcessAddToEntryQueue(ActiveEntry* entry) {
  DCHECK(!entry->add_to_entry_queue.empty());

  // Note the entry may be new or may already have a response body written to
  // it. In both cases, a transaction needs to wait since only one transaction
  // can be in the headers phase at a time.
  if (entry->headers_transaction) {
    return;
  }
  Transaction* transaction = entry->add_to_entry_queue.front();
  entry->add_to_entry_queue.erase(entry->add_to_entry_queue.begin());
  entry->headers_transaction = transaction;

  transaction->io_callback().Run(OK);
}

HttpCache::ParallelWritingPattern HttpCache::CanTransactionJoinExistingWriters(
    Transaction* transaction) {
  if (transaction->method() != "GET")
    return PARALLEL_WRITING_NOT_JOIN_METHOD_NOT_GET;
  if (transaction->partial())
    return PARALLEL_WRITING_NOT_JOIN_RANGE;
  if (transaction->mode() == Transaction::READ)
    return PARALLEL_WRITING_NOT_JOIN_READ_ONLY;
  if (transaction->GetResponseInfo()->headers &&
      transaction->GetResponseInfo()->headers->GetContentLength() >
          disk_cache_->MaxFileSize())
    return PARALLEL_WRITING_NOT_JOIN_TOO_BIG_FOR_CACHE;
  return PARALLEL_WRITING_JOIN;
}

void HttpCache::ProcessDoneHeadersQueue(ActiveEntry* entry) {
  ParallelWritingPattern writers_pattern;
  DCHECK(!entry->writers || entry->writers->CanAddWriters(&writers_pattern));
  DCHECK(!entry->done_headers_queue.empty());

  Transaction* transaction = entry->done_headers_queue.front();

  ParallelWritingPattern parallel_writing_pattern =
      CanTransactionJoinExistingWriters(transaction);
  if (IsWritingInProgress(entry)) {
    transaction->MaybeSetParallelWritingPatternForMetrics(
        parallel_writing_pattern);
    if (parallel_writing_pattern != PARALLEL_WRITING_JOIN) {
      // TODO(shivanisha): Returning from here instead of checking the next
      // transaction in the queue because the FIFO order is maintained
      // throughout, until it becomes a reader or writer. May be at this point
      // the ordering is not important but that would be optimizing a rare
      // scenario where write mode transactions are insterspersed with read-only
      // transactions.
      return;
    }
    AddTransactionToWriters(entry, transaction, parallel_writing_pattern);
  } else {  // no writing in progress
    if (transaction->mode() & Transaction::WRITE) {
      if (transaction->partial()) {
        if (entry->readers.empty())
          AddTransactionToWriters(entry, transaction, parallel_writing_pattern);
        else
          return;
      } else {
        // Add the transaction to readers since the response body should have
        // already been written. (If it was the first writer about to start
        // writing to the cache, it would have been added to writers in
        // DoneWithResponseHeaders, thus no writers here signify the response
        // was completely written).
        transaction->WriteModeTransactionAboutToBecomeReader();
        auto return_val = entry->readers.insert(transaction);
        DCHECK(return_val.second);
        transaction->MaybeSetParallelWritingPatternForMetrics(
            PARALLEL_WRITING_NONE_CACHE_READ);
      }
    } else {  // mode READ
      auto return_val = entry->readers.insert(transaction);
      DCHECK(return_val.second);
      transaction->MaybeSetParallelWritingPatternForMetrics(
          PARALLEL_WRITING_NONE_CACHE_READ);
    }
  }

  // Post another task to give a chance to more transactions to either join
  // readers or another transaction to start parallel validation.
  ProcessQueuedTransactions(entry);

  entry->done_headers_queue.erase(entry->done_headers_queue.begin());
  transaction->io_callback().Run(OK);
}

void HttpCache::AddTransactionToWriters(
    ActiveEntry* entry,
    Transaction* transaction,
    ParallelWritingPattern parallel_writing_pattern) {
  if (!entry->writers) {
    entry->writers = std::make_unique<Writers>(this, entry);
    transaction->MaybeSetParallelWritingPatternForMetrics(
        PARALLEL_WRITING_CREATE);
  } else {
    ParallelWritingPattern writers_pattern;
    DCHECK(entry->writers->CanAddWriters(&writers_pattern));
    DCHECK_EQ(PARALLEL_WRITING_JOIN, writers_pattern);
  }

  Writers::TransactionInfo info(transaction->partial(),
                                transaction->is_truncated(),
                                *(transaction->GetResponseInfo()));

  entry->writers->AddTransaction(transaction, parallel_writing_pattern,
                                 transaction->priority(), info);
}

bool HttpCache::CanTransactionWriteResponseHeaders(ActiveEntry* entry,
                                                   Transaction* transaction,
                                                   bool is_partial,
                                                   bool is_match) const {
  // If |transaction| is the current writer, do nothing. This can happen for
  // range requests since they can go back to headers phase after starting to
  // write.
  if (entry->writers && entry->writers->HasTransaction(transaction)) {
    DCHECK(is_partial);
    return true;
  }

  if (transaction != entry->headers_transaction)
    return false;

  if (!(transaction->mode() & Transaction::WRITE))
    return false;

  // If its not a match then check if it is the transaction responsible for
  // writing the response body.
  if (!is_match) {
    return (!entry->writers || entry->writers->IsEmpty()) &&
           entry->done_headers_queue.empty() && entry->readers.empty();
  }

  return true;
}

bool HttpCache::IsWritingInProgress(ActiveEntry* entry) const {
  return entry->writers.get();
}

LoadState HttpCache::GetLoadStateForPendingTransaction(
    const Transaction* transaction) {
  auto i = active_entries_.find(transaction->key());
  if (i == active_entries_.end()) {
    // If this is really a pending transaction, and it is not part of
    // active_entries_, we should be creating the backend or the entry.
    return LOAD_STATE_WAITING_FOR_CACHE;
  }

  Writers* writers = i->second->writers.get();
  return !writers ? LOAD_STATE_WAITING_FOR_CACHE : writers->GetLoadState();
}

void HttpCache::RemovePendingTransaction(Transaction* transaction) {
  auto i = active_entries_.find(transaction->key());
  bool found = false;
  if (i != active_entries_.end())
    found = RemovePendingTransactionFromEntry(i->second.get(), transaction);

  if (found)
    return;

  if (building_backend_) {
    auto j = pending_ops_.find(std::string());
    if (j != pending_ops_.end())
      found = RemovePendingTransactionFromPendingOp(j->second, transaction);

    if (found)
      return;
  }

  auto j = pending_ops_.find(transaction->key());
  if (j != pending_ops_.end())
    found = RemovePendingTransactionFromPendingOp(j->second, transaction);

  if (found)
    return;

  for (auto k = doomed_entries_.begin(); k != doomed_entries_.end() && !found;
       ++k) {
    found = RemovePendingTransactionFromEntry(k->first, transaction);
  }

  DCHECK(found) << "Pending transaction not found";
}

bool HttpCache::RemovePendingTransactionFromEntry(ActiveEntry* entry,
                                                  Transaction* transaction) {
  TransactionList& add_to_entry_queue = entry->add_to_entry_queue;

  auto j =
      find(add_to_entry_queue.begin(), add_to_entry_queue.end(), transaction);
  if (j == add_to_entry_queue.end())
    return false;

  add_to_entry_queue.erase(j);
  return true;
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

void HttpCache::OnProcessQueuedTransactions(ActiveEntry* entry) {
  entry->will_process_queued_transactions = false;

  // Note that this function should only invoke one transaction's IO callback
  // since its possible for IO callbacks' consumers to destroy the cache/entry.

  // If no one is interested in this entry, then we can deactivate it.
  if (entry->SafeToDestroy()) {
    DestroyEntry(entry);
    return;
  }

  if (entry->done_headers_queue.empty() && entry->add_to_entry_queue.empty())
    return;

  // To maintain FIFO order of transactions, done_headers_queue should be
  // checked for processing before add_to_entry_queue.

  // If another transaction is writing the response, let validated transactions
  // wait till the response is complete. If the response is not yet started, the
  // done_headers_queue transaction should start writing it.
  if (!entry->done_headers_queue.empty()) {
    ParallelWritingPattern reason = PARALLEL_WRITING_NONE;
    if (entry->writers && !entry->writers->CanAddWriters(&reason)) {
      if (reason != PARALLEL_WRITING_NONE) {
        for (auto* done_headers_transaction : entry->done_headers_queue) {
          done_headers_transaction->MaybeSetParallelWritingPatternForMetrics(
              reason);
        }
      }
    } else {
      ProcessDoneHeadersQueue(entry);
      return;
    }
  }

  if (!entry->add_to_entry_queue.empty())
    ProcessAddToEntryQueue(entry);
}

void HttpCache::OnIOComplete(int result, PendingOp* pending_op) {
  WorkItemOperation op = pending_op->writer->operation();

  // Completing the creation of the backend is simpler than the other cases.
  if (op == WI_CREATE_BACKEND)
    return OnBackendCreated(result, pending_op);

  std::unique_ptr<WorkItem> item = std::move(pending_op->writer);
  bool try_restart_requests = false;

  ActiveEntry* entry = nullptr;
  std::string key;
  if (result == OK) {
    if (op == WI_DOOM_ENTRY) {
      // Anything after a Doom has to be restarted.
      try_restart_requests = true;
    } else if (item->IsValid()) {
      key = pending_op->entry->GetKey();
      entry = ActivateEntry(pending_op->entry, pending_op->entry_opened);
    } else {
      // The writer transaction is gone.
      if (!pending_op->entry_opened)
        pending_op->entry->Doom();

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
  // to move the callback used to be a CancelableCallback. By the way, for this
  // to happen the action (to cancel B) has to be synchronous to the
  // notification for request A.
  WorkItemList pending_items;
  pending_items.swap(pending_op->pending_queue);
  DeletePendingOp(pending_op);

  item->NotifyTransaction(result, entry);

  while (!pending_items.empty()) {
    item = std::move(pending_items.front());
    pending_items.pop_front();

    if (item->operation() == WI_DOOM_ENTRY) {
      // A queued doom request is always a race.
      try_restart_requests = true;
    } else if (result == OK) {
      entry = FindActiveEntry(key);
      if (!entry)
        try_restart_requests = true;
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
void HttpCache::OnPendingOpComplete(const base::WeakPtr<HttpCache>& cache,
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
void HttpCache::OnPendingCreationOpComplete(
    const base::WeakPtr<HttpCache>& cache,
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

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&HttpCache::OnBackendCreated, GetWeakPtr(),
                                  result, pending_op));
  } else {
    building_backend_ = false;
    DeletePendingOp(pending_op);
  }

  // The cache may be gone when we return from the callback.
  if (!item->DoCallback(result, disk_cache_.get()))
    item->NotifyTransaction(result, nullptr);
}

}  // namespace net
