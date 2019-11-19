// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webtransport/quic_transport_connector_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"

namespace content {

QuicTransportConnectorImpl::QuicTransportConnectorImpl(
    int process_id,
    const url::Origin& origin,
    const net::NetworkIsolationKey& network_isolation_key)
    : process_id_(process_id),
      origin_(origin),
      network_isolation_key_(network_isolation_key) {}

void QuicTransportConnectorImpl::Connect(
    const GURL& url,
    mojo::PendingRemote<network::mojom::QuicTransportHandshakeClient>
        handshake_client) {
  RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
  if (!process) {
    return;
  }
  process->GetStoragePartition()->GetNetworkContext()->CreateQuicTransport(
      url, origin_, network_isolation_key_, std::move(handshake_client));
}

}  // namespace content
