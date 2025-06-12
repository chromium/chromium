// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/connection_change_observer_client_mojom_traits.h"

#include "mojo/public/cpp/bindings/message.h"
#include "net/base/reconnect_notifier.h"
#include "services/network/public/mojom/connection_change_observer_client.mojom-shared.h"

namespace mojo {

bool EnumTraits<network::mojom::NetworkChangeEvent, net::NetworkChangeEvent>::
    FromMojom(network::mojom::NetworkChangeEvent event_type,
              net::NetworkChangeEvent* out) {
  switch (event_type) {
    case network::mojom::NetworkChangeEvent::kConnected:
      *out = net::NetworkChangeEvent::kConnected;
      return true;
    case network::mojom::NetworkChangeEvent::kSoonToDisconnect:
      *out = net::NetworkChangeEvent::kSoonToDisconnect;
      return true;
    case network::mojom::NetworkChangeEvent::kDisconnected:
      *out = net::NetworkChangeEvent::kDisconnected;
      return true;
    case network::mojom::NetworkChangeEvent::kDefaultNetworkChanged:
      *out = net::NetworkChangeEvent::kDefaultNetworkChanged;
      return true;
  }
  return false;
}

network::mojom::NetworkChangeEvent EnumTraits<
    network::mojom::NetworkChangeEvent,
    net::NetworkChangeEvent>::ToMojom(net::NetworkChangeEvent event_type) {
  switch (event_type) {
    case net::NetworkChangeEvent::kConnected:
      return network::mojom::NetworkChangeEvent::kConnected;
    case net::NetworkChangeEvent::kSoonToDisconnect:
      return network::mojom::NetworkChangeEvent::kSoonToDisconnect;
    case net::NetworkChangeEvent::kDisconnected:
      return network::mojom::NetworkChangeEvent::kDisconnected;
    case net::NetworkChangeEvent::kDefaultNetworkChanged:
      return network::mojom::NetworkChangeEvent::kDefaultNetworkChanged;
  }
  NOTREACHED();
}

// static
bool StructTraits<network::mojom::ConnectionKeepAliveConfigDataView,
                  net::ConnectionKeepAliveConfig>::
    Read(network::mojom::ConnectionKeepAliveConfigDataView data,
         net::ConnectionKeepAliveConfig* out) {
  // Check if `ping_interval_sec` is smaller than the `idle_timeout_sec`
  // so that we will be able to send at least one ping before closing the
  // connection due to the idle timeout.
  if (data.ping_interval_in_seconds() > data.idle_timeout_in_seconds()) {
    return false;
  }

  out->ping_interval_in_seconds = data.ping_interval_in_seconds();
  out->idle_timeout_in_seconds = data.idle_timeout_in_seconds();
  out->enable_connection_keep_alive = data.enable_connection_keep_alive();

  return data.ReadQuicConnectionOptions(&out->quic_connection_options);
}

}  // namespace mojo
