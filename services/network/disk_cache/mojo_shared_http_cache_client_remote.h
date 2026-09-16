// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_DISK_CACHE_MOJO_SHARED_HTTP_CACHE_CLIENT_REMOTE_H_
#define SERVICES_NETWORK_DISK_CACHE_MOJO_SHARED_HTTP_CACHE_CLIENT_REMOTE_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/disk_cache/sql/shared_cache_client_remote.h"
#include "services/network/public/mojom/shared_http_cache_client.mojom.h"

namespace network {

// Mojo-based implementation of `disk_cache::SharedCacheClientRemote`.
//
// Bridges the `disk_cache::SqlBackendImpl` in the network service with a
// `SharedHttpCacheClient` in the renderer process (or browser process in tests)
// via Mojo interfaces.
//
// Upon construction, it binds a `mojom::SharedHttpCacheClientFactory` remote
// and creates a pending `mojom::SharedHttpCacheClient` pipe. When `Initialize`
// is called with the read-only SQLite database file handles, it sends both the
// file set and the client receiver endpoint to the factory, completing the
// handshake so subsequent `OnResourcesAdded` notifications can be delivered.
class COMPONENT_EXPORT(NETWORK_SERVICE) MojoSharedHttpCacheClientRemote
    : public disk_cache::SharedCacheClientRemote {
 public:
  explicit MojoSharedHttpCacheClientRemote(
      mojo::PendingRemote<mojom::SharedHttpCacheClientFactory> pending_remote);
  ~MojoSharedHttpCacheClientRemote() override;

  MojoSharedHttpCacheClientRemote(const MojoSharedHttpCacheClientRemote&) =
      delete;
  MojoSharedHttpCacheClientRemote& operator=(
      const MojoSharedHttpCacheClientRemote&) = delete;

  // disk_cache::SharedCacheClientRemote implementation:
  void Initialize(sqlite_vfs::PendingFileSet pending_file_set) override;
  void OnResourcesAdded(const std::vector<uint32_t>& new_hashes) override;
  void SetDisconnectHandler(base::OnceClosure disconnect_handler) override;

  bool is_factory_connected_for_testing() const {
    return factory_remote_.is_connected();
  }

 private:
  void OnDisconnected();

  // Remote endpoint to the client factory, used once during `Initialize` to
  // pass the read-only SQLite database file handles and bind `client_remote_`.
  mojo::Remote<mojom::SharedHttpCacheClientFactory> factory_remote_;

  // Remote endpoint to the active client, used to notify it when new cache
  // entries are added.
  mojo::Remote<mojom::SharedHttpCacheClient> client_remote_;

  // Held until `Initialize` is called and passed to `CreateClient` on
  // `factory_remote_`.
  mojo::PendingReceiver<mojom::SharedHttpCacheClient> client_receiver_;

  // Invoked when either `factory_remote_` (before `Initialize`) or
  // `client_remote_` disconnects.
  base::OnceClosure disconnect_handler_;

  base::WeakPtrFactory<MojoSharedHttpCacheClientRemote> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_DISK_CACHE_MOJO_SHARED_HTTP_CACHE_CLIENT_REMOTE_H_
