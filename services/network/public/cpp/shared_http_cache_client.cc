// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/shared_http_cache_client.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/hash/hash.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "components/sqlite_vfs/pending_file_set.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/data_buffer_factory.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "url/gurl.h"

namespace network {

namespace {

// A thread-safe set of persistent URL hashes.
// Used to quickly determine whether a requested URL might exist in the
// shared HTTP cache without thread hopping or disk I/O.
class ThreadSafeSet : public base::RefCountedThreadSafe<ThreadSafeSet> {
 public:
  ThreadSafeSet();
  ThreadSafeSet(const ThreadSafeSet&) = delete;
  ThreadSafeSet& operator=(const ThreadSafeSet&) = delete;

  void Initialize();
  void Insert(const std::vector<uint32_t>& new_hashes);
  bool ShouldEarlyReturn(uint32_t hash);

 private:
  friend class base::RefCountedThreadSafe<ThreadSafeSet>;
  ~ThreadSafeSet();

  base::Lock lock_;
  // TODO(crbug.com/473666511): Using an unbounded hash set can lead to high
  // memory usage in long-lived renderers. Use base::LRUCacheSet with a size
  // limit to cap memory consumption.
  std::optional<absl::flat_hash_set<uint32_t>> set_ GUARDED_BY(lock_);
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

// Implements network::mojom::SharedHttpCacheClientFactory on
// `database_task_runner_`.
// Receives `pending_file_set` to initialize the database reader (in a follow-up
// CL), marks the cache initialized, and creates `CacheClient` on
// `client_task_runner_`.
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
  ~DatabaseBackend() override = default;
  DatabaseBackend(const DatabaseBackend&) = delete;
  DatabaseBackend& operator=(const DatabaseBackend&) = delete;

  // network::mojom::SharedHttpCacheClientFactory:
  void CreateClient(sqlite_vfs::PendingFileSet pending_file_set,
                    mojo::PendingReceiver<network::mojom::SharedHttpCacheClient>
                        client_receiver) override {
    if (client_created_) {
      receiver_.ReportBadMessage("Duplicate CreateClient call");
      return;
    }
    client_created_ = true;
    shared_state_->Initialize();
    cache_client_.emplace(client_task_runner_, std::move(client_receiver),
                          shared_state_);
    // TODO(crbug.com/473666511): Initializing the SQLite database reader using
    // `pending_file_set` on `database_task_runner_` will be implemented in a
    // follow-up CL.
    if (on_db_reader_initialized_callback_) {
      std::move(on_db_reader_initialized_callback_).Run();
    }
  }

 private:
  mojo::Receiver<network::mojom::SharedHttpCacheClientFactory> receiver_;
  const scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  const scoped_refptr<ThreadSafeSet> shared_state_;
  base::OnceClosure on_db_reader_initialized_callback_;
  bool client_created_ = false;
  base::SequenceBound<CacheClient> cache_client_;
};

class SharedHttpCacheClientImpl : public SharedHttpCacheClient {
 public:
  SharedHttpCacheClientImpl(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      scoped_refptr<base::SequencedTaskRunner> database_task_runner);

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
  // In this initial CL, SQLite database reading is not yet implemented (it will
  // be added in a follow-up CL). Although named `database_task_runner_`, direct
  // database processing is not performed in this CL. However,
  // `database_task_runner_` is already used here to host `DatabaseBackend` so
  // that `SharedHttpCacheClientFactory` runs on the sequence where database
  // operations will execute once implemented.
  const scoped_refptr<base::SequencedTaskRunner> database_task_runner_;
  const scoped_refptr<ThreadSafeSet> shared_state_;
  base::SequenceBound<DatabaseBackend> database_backend_;
};

ThreadSafeSet::ThreadSafeSet() = default;
ThreadSafeSet::~ThreadSafeSet() = default;

void ThreadSafeSet::Initialize() {
  base::AutoLock auto_lock(lock_);
  init_called_ = true;
}
void ThreadSafeSet::Insert(const std::vector<uint32_t>& new_hashes) {
  base::AutoLock auto_lock(lock_);
  if (!set_.has_value()) {
    set_ = absl::flat_hash_set<uint32_t>();
  }
  for (auto hash : new_hashes) {
    set_->insert(hash);
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
    // TODO(crbug.com/473666511): The network service should unconditionally
    // signal when initial hashes are loaded even on empty caches so that `set_`
    // is initialized, avoiding querying SQLite on empty caches. This will be
    // addressed in a follow-up CL.
    return false;
  }
  return !set_->contains(hash);
}

SharedHttpCacheClientImpl::SharedHttpCacheClientImpl(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    scoped_refptr<base::SequencedTaskRunner> database_task_runner)
    : client_task_runner_(std::move(client_task_runner)),
      database_task_runner_(std::move(database_task_runner)),
      shared_state_(base::MakeRefCounted<ThreadSafeSet>()) {
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
  if (request.is_revalidating || request.revalidation_etag ||
      request.revalidation_last_modified) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  if (ShouldEarlyReturn(request.url)) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  // TODO(crbug.com/473666511): Database lookup will be implemented in a
  // follow-up CL.
  callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
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
    base::OnceClosure on_db_reader_initialized_callback) {
  auto impl = base::MakeRefCounted<SharedHttpCacheClientImpl>(
      std::move(client_task_runner), std::move(database_task_runner));
  impl->Init(std::move(pending_receiver),
             std::move(on_db_reader_initialized_callback));
  return impl;
}

}  // namespace network
