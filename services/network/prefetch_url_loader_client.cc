// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/prefetch_url_loader_client.h"

#include <functional>
#include <optional>

#include "base/check.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/prefetch_cache.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network {

namespace {

// Returns true for a response code that we'd like to use.
bool IsNiceResponseCode(int response_code) {
  const int first_digit = response_code / 100;
  // 2xx response codes are "OK", "No content", etc.
  // 3xx response codes are "Moved permanently", "Not modified", etc.
  return first_digit == 2 || first_digit == 3;
}

// Returns true if this prefetch has attributes that make it unattractive to
// use.
bool ShouldAbandonPrefetch(const mojom::URLResponseHead& head) {
  CHECK(head.headers);
  const net::HttpResponseHeaders& headers = *head.headers;
  return !IsNiceResponseCode(headers.response_code()) ||
         headers.HasHeaderValue("vary", "purpose") ||
         headers.HasHeaderValue("vary", "sec-purpose") ||
         headers.HasHeaderValue("cache-control", "no-store");
}

}  // namespace

PrefetchURLLoaderClient::PrefetchURLLoaderClient(
    base::PassKey<PrefetchCache>,
    const net::NetworkIsolationKey& nik,
    const ResourceRequest& request,
    base::TimeTicks expiry_time,
    PrefetchCache* prefetch_cache)
    : request_(request),
      network_isolation_key_(nik),
      expiry_time_(expiry_time),
      prefetch_cache_(prefetch_cache) {
  CHECK(prefetch_cache_);
}

PrefetchURLLoaderClient::~PrefetchURLLoaderClient() = default;

mojo::PendingReceiver<mojom::URLLoader>
PrefetchURLLoaderClient::GetURLLoaderPendingReceiver() {
  CHECK(!url_loader_);
  auto pending_receiver = url_loader_.BindNewPipeAndPassReceiver();
  // This use of base::Unretained() is safe because the callback will not be
  // called after `url_loader_` is destroyed.
  url_loader_.set_disconnect_handler(base::BindOnce(
      &PrefetchURLLoaderClient::OnDisconnect, base::Unretained(this)));
  return pending_receiver;
}

mojo::PendingRemote<mojom::URLLoaderClient>
PrefetchURLLoaderClient::BindNewPipeAndPassRemote() {
  CHECK(!receiver_.is_bound());
  auto pending_remote = receiver_.BindNewPipeAndPassRemote();
  // This use of base::Unretained() is safe because the callback will not be
  // called after `receiver_` is destroyed.
  receiver_.set_disconnect_handler(base::BindOnce(
      &PrefetchURLLoaderClient::OnDisconnect, base::Unretained(this)));
  return pending_remote;
}

void PrefetchURLLoaderClient::SetClient(
    mojo::PendingRemote<mojom::URLLoaderClient> client) {
  // Remove ourselves from the PrefetchCache.
  prefetch_cache_->Consume(this);

  real_client_.Bind(std::move(client));
  // This use of base::Unretained is safe because the disconnect handler will
  // not be called after `real_client_` is destroyed.
  real_client_.set_disconnect_handler(base::BindOnce(
      &PrefetchURLLoaderClient::OnDisconnect, base::Unretained(this)));
  // Move the vector to a local just to be safe against re-entrancy.
  std::vector<base::OnceClosure> pending_callbacks =
      std::move(pending_callbacks_);
  // `pending_callbacks_` shouldn't be used again, but if it is, avoid undefined
  // behavior.
  pending_callbacks_.clear();
  for (auto&& callback : pending_callbacks) {
    std::move(callback).Run();
  }
  if (disconnected_) {
    OnDisconnect();
    // `this` is deleted here.
  }
}

void PrefetchURLLoaderClient::OnReceiveEarlyHints(
    mojom::EarlyHintsPtr early_hints) {
  ForwardOrRecord(&mojom::URLLoaderClient::OnReceiveEarlyHints,
                  std::move(early_hints));
}

void PrefetchURLLoaderClient::OnReceiveResponse(
    mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  // If a render process is already consuming this prefetch, it is too late to
  // decide we can't use it.
  if (!real_client_.is_bound() && ShouldAbandonPrefetch(*head)) {
    prefetch_cache_->Erase(this);
    return;
  }

  ForwardOrRecord(&mojom::URLLoaderClient::OnReceiveResponse, std::move(head),
                  std::move(body), std::move(cached_metadata));
}

void PrefetchURLLoaderClient::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    mojom::URLResponseHeadPtr head) {
  ForwardOrRecord(&mojom::URLLoaderClient::OnReceiveRedirect, redirect_info,
                  std::move(head));
}

void PrefetchURLLoaderClient::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  ForwardOrRecord(&mojom::URLLoaderClient::OnUploadProgress, current_position,
                  total_size, std::move(callback));
}

void PrefetchURLLoaderClient::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  ForwardOrRecord(&mojom::URLLoaderClient::OnTransferSizeUpdated,
                  transfer_size_diff);
}

void PrefetchURLLoaderClient::OnComplete(
    const URLLoaderCompletionStatus& status) {
  ForwardOrRecord(&mojom::URLLoaderClient::OnComplete, status);
}

void PrefetchURLLoaderClient::OnDisconnect() {
  disconnected_ = true;
  if (real_client_.is_bound()) {
    // We don't need to queue the disconnect because all queued messages will
    // already have been sent to `real_client_`.
    // Just disconnect by deleting this object.
    prefetch_cache_->Erase(this);
  }
}

template <typename Method, typename... Args>
void PrefetchURLLoaderClient::ForwardOrRecord(Method method, Args... args) {
  if (real_client_.is_bound()) {
    ForwardToRealClient(method, std::forward<Args>(args)...);
  } else {
    // This use of `base::Unretained` is safe because the callback is owned by
    // this object.
    pending_callbacks_.push_back(base::BindOnce(
        &PrefetchURLLoaderClient::ForwardToRealClient<Method, Args...>,
        base::Unretained(this), method, std::forward<Args>(args)...));
  }
}

template <typename Method, typename... Args>
void PrefetchURLLoaderClient::ForwardToRealClient(Method method, Args... args) {
  std::invoke(method, real_client_, std::forward<Args>(args)...);
}

}  // namespace network
