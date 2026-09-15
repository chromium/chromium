// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/shared_http_cache_client.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/containers/hashing_lru_cache.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/hash/hash.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "components/sqlite_vfs/pending_file_set.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/url_util.h"
#include "net/disk_cache/sql/sql_shared_cache_isolated_database_reader.h"
#include "net/filter/filter_source_stream.h"
#include "net/http/http_response_info.h"
#include "services/network/public/cpp/content_decoding_util.h"
#include "services/network/public/cpp/data_buffer_factory.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace network {

namespace {

// A thread-safe LRU cache set of persistent URL hashes.
// Used to quickly determine whether a requested URL might exist in the
// shared HTTP cache without thread hopping or disk I/O.
class ThreadSafeSet : public base::RefCountedThreadSafe<ThreadSafeSet> {
 public:
  explicit ThreadSafeSet(size_t max_size);
  ThreadSafeSet(const ThreadSafeSet&) = delete;
  ThreadSafeSet& operator=(const ThreadSafeSet&) = delete;

  void Initialize();
  void Insert(const std::vector<uint32_t>& new_hashes);
  bool ShouldEarlyReturn(uint32_t hash);

 private:
  friend class base::RefCountedThreadSafe<ThreadSafeSet>;
  ~ThreadSafeSet();

  const size_t max_size_;
  base::Lock lock_;
  std::optional<base::HashingLRUCacheSet<uint32_t>> set_ GUARDED_BY(lock_);
  bool init_called_ GUARDED_BY(lock_) = false;
};

// Implements network::mojom::SharedHttpCacheClient on `client_task_runner_`.
// Receives cache hash updates and inserts them into `shared_state_`.
class CacheClient : public network::mojom::SharedHttpCacheClient {
 public:
  CacheClient(mojo::PendingReceiver<network::mojom::SharedHttpCacheClient>
                  pending_receiver,
              scoped_refptr<ThreadSafeSet> shared_state)
      : receiver_(this, std::move(pending_receiver)),
        shared_state_(std::move(shared_state)) {}
  ~CacheClient() override = default;
  CacheClient(const CacheClient&) = delete;
  CacheClient& operator=(const CacheClient&) = delete;

  // network::mojom::SharedHttpCacheClient:
  void OnResourcesAdded(const std::vector<uint32_t>& new_hashes) override {
    shared_state_->Insert(new_hashes);
  }

 private:
  mojo::Receiver<network::mojom::SharedHttpCacheClient> receiver_;
  const scoped_refptr<ThreadSafeSet> shared_state_;
};

// Implements network::mojom::SharedHttpCacheClientFactory and executes SQLite
// database reads on `database_task_runner_`.
class DatabaseBackend : public network::mojom::SharedHttpCacheClientFactory {
 public:
  DatabaseBackend(
      mojo::PendingReceiver<network::mojom::SharedHttpCacheClientFactory>
          pending_receiver,
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      scoped_refptr<ThreadSafeSet> shared_state,
      base::OnceClosure on_db_reader_initialized_callback)
      : receiver_(this, std::move(pending_receiver)),
        client_task_runner_(std::move(client_task_runner)),
        shared_state_(std::move(shared_state)),
        on_db_reader_initialized_callback_(
            std::move(on_db_reader_initialized_callback)) {}
  ~DatabaseBackend() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }
  DatabaseBackend(const DatabaseBackend&) = delete;
  DatabaseBackend& operator=(const DatabaseBackend&) = delete;

  // network::mojom::SharedHttpCacheClientFactory:
  void CreateClient(sqlite_vfs::PendingFileSet pending_file_set,
                    mojo::PendingReceiver<network::mojom::SharedHttpCacheClient>
                        client_receiver) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (client_created_) {
      receiver_.ReportBadMessage("Duplicate CreateClient call");
      return;
    }
    client_created_ = true;
    db_reader_ =
        std::make_unique<disk_cache::SqlSharedCacheIsolatedDatabaseReader>(
            std::move(pending_file_set));
    shared_state_->Initialize();
    cache_client_.emplace(client_task_runner_, std::move(client_receiver),
                          shared_state_);
    if (on_db_reader_initialized_callback_) {
      std::move(on_db_reader_initialized_callback_).Run();
    }
  }

  void FindResource(
      const GURL& url,
      base::TimeTicks request_start,
      base::Time request_start_time,
      scoped_refptr<DataBufferFactory> data_buffer_factory,
      base::OnceCallback<void(std::optional<SharedHttpCacheClient::Response>)>
          callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(db_reader_);
    const base::TimeTicks send_start = base::TimeTicks::Now();
    const base::TimeTicks send_end = send_start;
    auto response = db_reader_->ReadResponse(url.spec());
    if (!response.has_value()) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    CHECK(data_buffer_factory);
    const auto body_size = response->GetBodySize();
    auto body = data_buffer_factory->AllocateDataBuffer(body_size);
    if (!response->ReadBody(body->data())) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::USER_BLOCKING},
        base::BindOnce(
            &DatabaseBackend::ParseAndDecode, std::move(data_buffer_factory),
            request_start, request_start_time, send_start, send_end,
            response->TakeHeaders(), std::move(body), std::move(callback)));
  }

 private:
  static void ParseAndDecode(
      scoped_refptr<DataBufferFactory> data_buffer_factory,
      base::TimeTicks request_start,
      base::Time request_start_time,
      base::TimeTicks send_start,
      base::TimeTicks send_end,
      std::vector<uint8_t> headers_pickle,
      std::unique_ptr<DataBuffer> body,
      base::OnceCallback<void(std::optional<SharedHttpCacheClient::Response>)>
          callback);

  mojo::Receiver<network::mojom::SharedHttpCacheClientFactory> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);
  const scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  const scoped_refptr<ThreadSafeSet> shared_state_;
  base::OnceClosure on_db_reader_initialized_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool client_created_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  base::SequenceBound<CacheClient> cache_client_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<disk_cache::SqlSharedCacheIsolatedDatabaseReader> db_reader_
      GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
};

// static
void DatabaseBackend::ParseAndDecode(
    scoped_refptr<DataBufferFactory> data_buffer_factory,
    base::TimeTicks request_start,
    base::Time request_start_time,
    base::TimeTicks send_start,
    base::TimeTicks send_end,
    std::vector<uint8_t> headers_pickle,
    std::unique_ptr<DataBuffer> body,
    base::OnceCallback<void(std::optional<SharedHttpCacheClient::Response>)>
        callback) {
  CHECK(body);
  const base::TimeTicks receive_headers_start = base::TimeTicks::Now();
  base::PickleIterator pickle_iter = base::PickleIterator::WithData(
      base::as_bytes(base::span(headers_pickle)));
  auto response_info = std::make_unique<net::HttpResponseInfo>();
  bool response_truncated = false;
  if (!response_info->InitFromPickle(pickle_iter, &response_truncated) ||
      !response_info->headers) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  const base::TimeTicks receive_headers_end = base::TimeTicks::Now();

  // TODO(crbug.com/473666511): Add freshness check (e.g. Cache-Control:
  // max-age, Expires, Age, Date headers) so that stale entries are not served
  // directly and are instead revalidated via the network service.
  //
  // TODO(crbug.com/473666511): Support Stale-While-Revalidate (SWR) by serving
  // the cached response while triggering an asynchronous revalidation request
  // to the network service.
  //
  // TODO(crbug.com/473666511): Implement read-time security checks (CORS,
  // ORB, CORP, SRI) to ensure responses stored under different request modes
  // or before ORB rule updates are safe to serve for the current request.

  network::mojom::URLResponseHeadPtr head =
      network::mojom::URLResponseHead::New();
  head->headers = response_info->headers;
  head->request_time = response_info->request_time;
  head->response_time = response_info->response_time;
  head->original_response_time = response_info->original_response_time;
  head->content_length = body->data().size();
  head->headers->GetMimeTypeAndCharset(&head->mime_type, &head->charset);
  head->was_fetched_via_cache = true;
  head->remote_endpoint = response_info->remote_endpoint;
  head->was_fetched_via_spdy = response_info->was_fetched_via_spdy;
  head->was_alpn_negotiated = response_info->was_alpn_negotiated;
  head->alpn_negotiated_protocol = response_info->alpn_negotiated_protocol;
  head->alternate_protocol_usage = response_info->alternate_protocol_usage;
  head->connection_info = response_info->connection_info;
  head->network_accessed = false;
  head->request_start = request_start;
  head->response_start = receive_headers_end;
  head->encoded_body_length = network::mojom::EncodedBodyLength::New(
      response_info->encoded_body_size.has_value()
          ? response_info->encoded_body_size->InBytes()
          : body->data().size());
  head->encoded_data_length = response_info->headers->raw_headers().size() +
                              head->encoded_body_length->value;

  head->load_timing.request_start = request_start;
  head->load_timing.request_start_time = request_start_time;
  head->load_timing.send_start = send_start;
  head->load_timing.send_end = send_end;
  head->load_timing.receive_headers_start = receive_headers_start;
  head->load_timing.receive_headers_end = receive_headers_end;

  const auto content_encoding_types =
      net::FilterSourceStream::GetContentEncodingTypes(*response_info->headers);
  if (content_encoding_types.empty()) {
    auto buffers = data_buffer_factory->CreateDataBufferList();
    CHECK(buffers);
    if (!body->data().empty()) {
      buffers->Append(std::move(body));
    }
    std::move(callback).Run(
        SharedHttpCacheClient::Response(std::move(head), std::move(buffers)));
    return;
  }
  auto buffers = ContentDecodingUtil::Decode(
      body->data(), content_encoding_types, *data_buffer_factory.get());
  if (!buffers) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(
      SharedHttpCacheClient::Response(std::move(head), std::move(buffers)));
}

class SharedHttpCacheClientImpl : public SharedHttpCacheClient {
 public:
  SharedHttpCacheClientImpl(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      scoped_refptr<base::SequencedTaskRunner> database_task_runner,
      size_t max_cached_url_hashes);

  SharedHttpCacheClientImpl(const SharedHttpCacheClientImpl&) = delete;
  SharedHttpCacheClientImpl& operator=(const SharedHttpCacheClientImpl&) =
      delete;

  void Init(mojo::PendingReceiver<network::mojom::SharedHttpCacheClientFactory>
                pending_receiver,
            base::OnceClosure on_db_reader_initialized_callback);
  void Find(
      const network::ResourceRequest& request,
      scoped_refptr<DataBufferFactory> data_buffer_factory,
      base::OnceCallback<void(std::optional<Response>)> callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) override;

 private:
  friend class base::RefCountedThreadSafe<SharedHttpCacheClientImpl>;
  ~SharedHttpCacheClientImpl() override;

  bool ShouldEarlyReturn(const GURL& url);

  const scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> database_task_runner_;
  const scoped_refptr<ThreadSafeSet> shared_state_;
  base::SequenceBound<DatabaseBackend> database_backend_;
};

ThreadSafeSet::ThreadSafeSet(size_t max_size) : max_size_(max_size) {
  CHECK_GT(max_size_, 0u);
}
ThreadSafeSet::~ThreadSafeSet() = default;

void ThreadSafeSet::Initialize() {
  base::AutoLock auto_lock(lock_);
  init_called_ = true;
}
void ThreadSafeSet::Insert(const std::vector<uint32_t>& new_hashes) {
  base::AutoLock auto_lock(lock_);
  if (!set_.has_value()) {
    set_.emplace(max_size_);
  }
  for (uint32_t hash : new_hashes) {
    // `LRUCacheBase::Put(value_type&&)` only accepts rvalues.
    set_->Put(std::move(hash));
  }
}

bool ThreadSafeSet::ShouldEarlyReturn(uint32_t hash) {
  base::AutoLock auto_lock(lock_);
  if (!init_called_) {
    // TODO(crbug.com/473666511): Consider making early-return vs queuing before
    // database initialization configurable via a base::FeatureParam.
    return true;
  }
  if (!set_.has_value()) {
    return false;
  }
  return set_->Get(hash) == set_->end();
}

SharedHttpCacheClientImpl::SharedHttpCacheClientImpl(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    scoped_refptr<base::SequencedTaskRunner> database_task_runner,
    size_t max_cached_url_hashes)
    : client_task_runner_(std::move(client_task_runner)),
      database_task_runner_(std::move(database_task_runner)),
      shared_state_(
          base::MakeRefCounted<ThreadSafeSet>(max_cached_url_hashes)) {
  CHECK(client_task_runner_);
  CHECK(database_task_runner_);
}

SharedHttpCacheClientImpl::~SharedHttpCacheClientImpl() = default;

void SharedHttpCacheClientImpl::Init(
    mojo::PendingReceiver<network::mojom::SharedHttpCacheClientFactory>
        pending_receiver,
    base::OnceClosure on_db_reader_initialized_callback) {
  CHECK(pending_receiver);
  database_backend_ = base::SequenceBound<DatabaseBackend>(
      database_task_runner_, std::move(pending_receiver), client_task_runner_,
      shared_state_, std::move(on_db_reader_initialized_callback));
}

bool SharedHttpCacheClientImpl::ShouldEarlyReturn(const GURL& url) {
  if (!url.is_valid()) {
    return true;
  }
  const auto hash =
      base::PersistentHash(net::SimplifyUrlForRequest(url).spec());
  return shared_state_->ShouldEarlyReturn(hash);
}

void SharedHttpCacheClientImpl::Find(
    const network::ResourceRequest& request,
    scoped_refptr<DataBufferFactory> data_buffer_factory,
    base::OnceCallback<void(std::optional<Response>)> callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  // TODO(crbug.com/473666511): Ineligible requests for cache lookup (e.g.
  // non-GET HTTP methods, loading flags such as LOAD_DISABLE_CACHE, non-HTTP
  // schemes) must also be skipped here. Implement this in a follow-up CL.
  const base::TimeTicks request_start = base::TimeTicks::Now();
  const base::Time request_start_time = base::Time::Now();
  if (request.is_revalidating || request.revalidation_etag ||
      request.revalidation_last_modified) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  if (ShouldEarlyReturn(request.url)) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  database_backend_.AsyncCall(&DatabaseBackend::FindResource)
      .WithArgs(request.url, request_start, request_start_time,
                std::move(data_buffer_factory),
                base::BindPostTask(std::move(callback_task_runner),
                                   std::move(callback)));
}
}  // namespace

SharedHttpCacheClient::Response::Response(
    network::mojom::URLResponseHeadPtr head,
    std::unique_ptr<DataBufferList> body)
    : head(std::move(head)), body(std::move(body)) {}
SharedHttpCacheClient::Response::~Response() = default;
SharedHttpCacheClient::Response::Response(Response&&) = default;
SharedHttpCacheClient::Response& SharedHttpCacheClient::Response::operator=(
    Response&&) = default;

// static
scoped_refptr<SharedHttpCacheClient> SharedHttpCacheClient::CreateAndInit(
    mojo::PendingReceiver<network::mojom::SharedHttpCacheClientFactory>
        pending_receiver,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    scoped_refptr<base::SequencedTaskRunner> database_task_runner,
    base::OnceClosure on_db_reader_initialized_callback,
    size_t max_cached_url_hashes) {
  auto impl = base::MakeRefCounted<SharedHttpCacheClientImpl>(
      std::move(client_task_runner), std::move(database_task_runner),
      max_cached_url_hashes);
  impl->Init(std::move(pending_receiver),
             std::move(on_db_reader_initialized_callback));
  return impl;
}

}  // namespace network
