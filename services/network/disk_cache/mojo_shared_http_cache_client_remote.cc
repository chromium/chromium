// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/disk_cache/mojo_shared_http_cache_client_remote.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"

namespace network {

MojoSharedHttpCacheClientRemote::MojoSharedHttpCacheClientRemote(
    mojo::PendingRemote<mojom::SharedHttpCacheClientFactory> pending_remote)
    : factory_remote_(std::move(pending_remote)),
      client_receiver_(client_remote_.BindNewPipeAndPassReceiver()) {
  CHECK(factory_remote_);
}

MojoSharedHttpCacheClientRemote::~MojoSharedHttpCacheClientRemote() = default;

void MojoSharedHttpCacheClientRemote::Initialize(
    sqlite_vfs::PendingFileSet pending_file_set) {
  // `SetDisconnectHandler()` must be called before `Initialize()`.
  CHECK(disconnect_handler_);
  CHECK(client_receiver_);
  factory_remote_->CreateClient(std::move(pending_file_set),
                                std::move(client_receiver_));
  // `SharedHttpCacheClientFactory` is single-use. Once `CreateClient()` is
  // called, `client_remote_` governs the client lifetime.
  factory_remote_.set_disconnect_handler(base::NullCallback());
}

void MojoSharedHttpCacheClientRemote::OnResourcesAdded(
    const std::vector<uint32_t>& new_hashes) {
  client_remote_->OnResourcesAdded(new_hashes);
}

void MojoSharedHttpCacheClientRemote::SetDisconnectHandler(
    base::OnceClosure disconnect_handler) {
  // `SetDisconnectHandler()` must be called before `Initialize()`.
  CHECK(!disconnect_handler_);
  CHECK(client_receiver_);
  disconnect_handler_ = std::move(disconnect_handler);

  // If `factory_remote_` already disconnected before `SetDisconnectHandler()`
  // was called, `set_disconnect_handler()` is a no-op. Post `OnDisconnected()`
  // asynchronously to avoid synchronous destruction inside the caller.
  if (!factory_remote_.is_connected()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&MojoSharedHttpCacheClientRemote::OnDisconnected,
                       weak_factory_.GetWeakPtr()));
    return;
  }
  factory_remote_.set_disconnect_handler(
      base::BindOnce(&MojoSharedHttpCacheClientRemote::OnDisconnected,
                     base::Unretained(this)));
  client_remote_.set_disconnect_handler(
      base::BindOnce(&MojoSharedHttpCacheClientRemote::OnDisconnected,
                     base::Unretained(this)));
}

void MojoSharedHttpCacheClientRemote::OnDisconnected() {
  if (disconnect_handler_) {
    std::move(disconnect_handler_).Run();
  }
}

}  // namespace network
