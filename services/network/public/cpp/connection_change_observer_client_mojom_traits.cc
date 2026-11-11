// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/connection_change_observer_client_mojom_traits.h"

#include <optional>

#include "base/notreached.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/base/reconnect_notifier.h"
#include "services/network/public/cpp/network_ipc_param_traits.h"
#include "services/network/public/mojom/connection_change_observer_client.mojom-shared.h"

namespace mojo {

net::NetworkChangeEvent
EnumTraits<network::mojom::NetworkChangeEvent, net::NetworkChangeEvent>::
    FromMojom(network::mojom::NetworkChangeEvent event_type) {
  switch (event_type) {
    case network::mojom::NetworkChangeEvent::kConnected:
      return net::NetworkChangeEvent::kConnected;
    case network::mojom::NetworkChangeEvent::kSoonToDisconnect:
      return net::NetworkChangeEvent::kSoonToDisconnect;
    case network::mojom::NetworkChangeEvent::kDisconnected:
      return net::NetworkChangeEvent::kDisconnected;
    case network::mojom::NetworkChangeEvent::kDefaultNetworkChanged:
      return net::NetworkChangeEvent::kDefaultNetworkChanged;
  }
  NOTREACHED();
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
bool StructTraits<network::mojom::QuicKeepAliveTimeoutsDataView,
                  network::QuicKeepAliveTimeouts>::
    Read(network::mojom::QuicKeepAliveTimeoutsDataView data,
         network::QuicKeepAliveTimeouts* out) {
  if (!data.ReadIdleTimeout(&out->idle_timeout) ||
      !data.ReadPingInterval(&out->ping_interval)) {
    return false;
  }
  // Null represents an unset `ping_interval`; reject non-null zero on the wire.
  if (out->idle_timeout.is_zero() ||
      (out->ping_interval.has_value() && out->ping_interval->is_zero())) {
    return false;
  }
  net::ConnectionKeepAliveConfig config;
  config.idle_timeout = out->idle_timeout;
  config.ping_interval = out->ping_interval.value_or(base::TimeDelta());
  return config.IsValid();
}

// static
bool StructTraits<network::mojom::ConnectionKeepAliveConfigDataView,
                  net::ConnectionKeepAliveConfig>::
    Read(network::mojom::ConnectionKeepAliveConfigDataView data,
         net::ConnectionKeepAliveConfig* out) {
  std::optional<network::QuicKeepAliveTimeouts> keep_alive_timeouts;
  std::optional<base::TimeDelta> initial_rtt;
  if (!data.ReadKeepAliveTimeouts(&keep_alive_timeouts) ||
      !data.ReadInitialRtt(&initial_rtt) ||
      !data.ReadQuicConnectionOptions(&out->quic_connection_options)) {
    return false;
  }
  if (keep_alive_timeouts.has_value()) {
    out->idle_timeout = keep_alive_timeouts->idle_timeout;
    out->ping_interval =
        keep_alive_timeouts->ping_interval.value_or(base::TimeDelta());
  } else {
    out->idle_timeout = base::TimeDelta();
    out->ping_interval = base::TimeDelta();
  }
  // Null represents an unset `initial_rtt`; reject non-null zero on the wire.
  if (initial_rtt.has_value()) {
    if (initial_rtt->is_zero()) {
      return false;
    }
    out->initial_rtt = *initial_rtt;
  } else {
    out->initial_rtt = base::TimeDelta();
  }
  out->enable_connection_keep_alive = data.enable_connection_keep_alive();
  return out->IsValid();
}

net::ConnectionEstablishmentInitiator
EnumTraits<network::mojom::ConnectionEstablishmentInitiator,
           net::ConnectionEstablishmentInitiator>::
    FromMojom(network::mojom::ConnectionEstablishmentInitiator initiator) {
  switch (initiator) {
    case network::mojom::ConnectionEstablishmentInitiator::kPreconnect:
      return net::ConnectionEstablishmentInitiator::kPreconnect;
    case network::mojom::ConnectionEstablishmentInitiator::kRequest:
      return net::ConnectionEstablishmentInitiator::kRequest;
  }
  NOTREACHED();
}

network::mojom::ConnectionEstablishmentInitiator
EnumTraits<network::mojom::ConnectionEstablishmentInitiator,
           net::ConnectionEstablishmentInitiator>::
    ToMojom(net::ConnectionEstablishmentInitiator initiator) {
  switch (initiator) {
    case net::ConnectionEstablishmentInitiator::kPreconnect:
      return network::mojom::ConnectionEstablishmentInitiator::kPreconnect;
    case net::ConnectionEstablishmentInitiator::kRequest:
      return network::mojom::ConnectionEstablishmentInitiator::kRequest;
  }
  NOTREACHED();
}

// static
bool StructTraits<network::mojom::EstablishedConnectionInfoDataView,
                  net::ConnectionChangeNotifier::EstablishedConnectionInfo>::
    Read(network::mojom::EstablishedConnectionInfoDataView data,
         net::ConnectionChangeNotifier::EstablishedConnectionInfo* out) {
  if (!data.ReadConnectionInfo(&out->connection_info)) {
    return false;
  }
  if (!data.ReadConnectionSetupTime(&out->connection_setup_time)) {
    return false;
  }
  if (!data.ReadInitiator(&out->initiator)) {
    return false;
  }
  return true;
}

}  // namespace mojo
