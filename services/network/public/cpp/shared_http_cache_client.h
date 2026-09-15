// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SHARED_HTTP_CACHE_CLIENT_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SHARED_HTTP_CACHE_CLIENT_H_

#include <stddef.h>

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/shared_http_cache_client.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network {
class DataBufferList;
class DataBufferFactory;
struct ResourceRequest;

// SharedHttpCacheClient provides client-side access to the SQLite-based shared
// HTTP cache in the renderer process (Renderer-Accessible HTTP Cache).
//
// It maintains a thread-safe in-memory LRU cache set of cached URL hashes to
// quickly determine if a resource might be available without disk I/O, and
// reads response data from the isolated SQLite database directly via
// sqlite_vfs, bypassing IPC to the network service on cache hits.
class COMPONENT_EXPORT(NETWORK_CPP) SharedHttpCacheClient
    : public base::RefCountedThreadSafe<SharedHttpCacheClient> {
 public:
  static constexpr size_t kDefaultMaxCachedUrlHashes = 10000;

  // Encapsulates the cached HTTP response headers and decoded body buffers.
  struct COMPONENT_EXPORT(NETWORK_CPP) Response {
    Response(network::mojom::URLResponseHeadPtr head,
             std::unique_ptr<DataBufferList> body);
    ~Response();
    Response(Response&&);
    Response& operator=(Response&&);
    Response(const Response&) = delete;
    Response& operator=(const Response&) = delete;

    network::mojom::URLResponseHeadPtr head;
    std::unique_ptr<DataBufferList> body;
  };

  // Creates and initializes a SharedHttpCacheClient instance.
  // `pending_receiver`: Mojo receiver for the SharedHttpCacheClientFactory
  // interface.
  // `client_task_runner`: Task runner for the client and receiving cache hash
  // updates.
  // `database_task_runner`: Task runner for executing SQLite database
  // operations and hosting the DatabaseBackend factory receiver.
  // `on_db_reader_initialized_callback`: Optional callback invoked on
  // `database_task_runner` when the database reader finishes initialization.
  // `max_cached_url_hashes`: Maximum number of URL hashes retained in the
  // in-memory LRU cache set to cap memory usage in long-lived renderers.
  static scoped_refptr<SharedHttpCacheClient> CreateAndInit(
      mojo::PendingReceiver<network::mojom::SharedHttpCacheClientFactory>
          pending_receiver,
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      scoped_refptr<base::SequencedTaskRunner> database_task_runner,
      base::OnceClosure on_db_reader_initialized_callback = base::OnceClosure(),
      size_t max_cached_url_hashes = kDefaultMaxCachedUrlHashes);

  SharedHttpCacheClient(const SharedHttpCacheClient&) = delete;
  SharedHttpCacheClient& operator=(const SharedHttpCacheClient&) = delete;

  // Queries the shared HTTP cache for `request`.
  //
  // If the request can be early-returned (e.g. if it is a revalidation request,
  // or if the URL is not present in the in-memory cache lookup set), `callback`
  // is invoked synchronously on the calling sequence with `std::nullopt`.
  //
  // Otherwise, the query is performed asynchronously. In this case, `callback`
  // is guaranteed to be posted to `callback_task_runner` with a `Response` on
  // cache hit, or with `std::nullopt` on cache miss, read error, or decoding
  // failure.
  virtual void Find(
      const network::ResourceRequest& request,
      scoped_refptr<DataBufferFactory> data_buffer_factory,
      base::OnceCallback<void(std::optional<Response>)> callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) = 0;

 protected:
  friend class base::RefCountedThreadSafe<SharedHttpCacheClient>;
  SharedHttpCacheClient() = default;
  virtual ~SharedHttpCacheClient() = default;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SHARED_HTTP_CACHE_CLIENT_H_
