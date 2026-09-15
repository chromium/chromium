// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/shared_http_cache_client.h"

#include <utility>

#include "services/network/public/cpp/data_buffer_factory.h"

namespace network {

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
  // SharedHttpCacheClient is not supported when the SQL disk cache backend
  // is disabled or unavailable on the target platform.
  return nullptr;
}

}  // namespace network
