// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_CHANGE_OBSERVER_CLIENT_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_CHANGE_OBSERVER_CLIENT_MOJOM_TRAITS_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/time/time.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/reconnect_notifier.h"
#include "services/network/public/cpp/load_timing_internal_info_mojom_traits.h"
#include "services/network/public/mojom/connection_change_observer_client.mojom-shared.h"
#include "services/network/public/mojom/network_types.mojom.h"

namespace network {

// C++ typemap representation of `network.mojom.QuicKeepAliveTimeouts` for
// serializing coupled QUIC idle and keep-alive ping timeouts in
// `net::ConnectionKeepAliveConfig`.
struct QuicKeepAliveTimeouts {
  base::TimeDelta idle_timeout;
  std::optional<base::TimeDelta> ping_interval;
};

}  // namespace network

namespace mojo {

template <>
struct EnumTraits<network::mojom::NetworkChangeEvent, net::NetworkChangeEvent> {
  static network::mojom::NetworkChangeEvent ToMojom(
      net::NetworkChangeEvent event_type);

  static net::NetworkChangeEvent FromMojom(
      network::mojom::NetworkChangeEvent event_type);
};

template <>
struct StructTraits<network::mojom::QuicKeepAliveTimeoutsDataView,
                    network::QuicKeepAliveTimeouts> {
  static base::TimeDelta idle_timeout(
      const network::QuicKeepAliveTimeouts& timeouts) {
    return timeouts.idle_timeout;
  }

  static std::optional<base::TimeDelta> ping_interval(
      const network::QuicKeepAliveTimeouts& timeouts) {
    return timeouts.ping_interval;
  }

  static bool Read(network::mojom::QuicKeepAliveTimeoutsDataView data,
                   network::QuicKeepAliveTimeouts* out);
};

template <>
struct StructTraits<network::mojom::ConnectionKeepAliveConfigDataView,
                    net::ConnectionKeepAliveConfig> {
 public:
  static std::optional<network::QuicKeepAliveTimeouts> keep_alive_timeouts(
      const net::ConnectionKeepAliveConfig& keep_alive_config) {
    if (keep_alive_config.idle_timeout.is_zero() &&
        keep_alive_config.ping_interval.is_zero()) {
      return std::nullopt;
    }
    return network::QuicKeepAliveTimeouts{
        keep_alive_config.idle_timeout,
        keep_alive_config.ping_interval.is_zero()
            ? std::nullopt
            : std::optional<base::TimeDelta>(keep_alive_config.ping_interval)};
  }

  static bool enable_connection_keep_alive(
      const net::ConnectionKeepAliveConfig& keep_alive_config) {
    return keep_alive_config.enable_connection_keep_alive;
  }

  static std::optional<base::TimeDelta> initial_rtt(
      const net::ConnectionKeepAliveConfig& keep_alive_config) {
    return keep_alive_config.initial_rtt.is_zero()
               ? std::nullopt
               : std::optional<base::TimeDelta>(keep_alive_config.initial_rtt);
  }

  static const std::string& quic_connection_options(
      const net::ConnectionKeepAliveConfig& keep_alive_config) {
    return keep_alive_config.quic_connection_options;
  }

  static bool Read(network::mojom::ConnectionKeepAliveConfigDataView data,
                   net::ConnectionKeepAliveConfig* out);
};

template <>
struct EnumTraits<network::mojom::ConnectionEstablishmentInitiator,
                  net::ConnectionEstablishmentInitiator> {
  static network::mojom::ConnectionEstablishmentInitiator ToMojom(
      net::ConnectionEstablishmentInitiator initiator);

  static net::ConnectionEstablishmentInitiator FromMojom(
      network::mojom::ConnectionEstablishmentInitiator initiator);
};

template <>
struct StructTraits<network::mojom::EstablishedConnectionInfoDataView,
                    net::ConnectionChangeNotifier::EstablishedConnectionInfo> {
 public:
  static net::NextProto connection_info(
      const net::ConnectionChangeNotifier::EstablishedConnectionInfo& info) {
    return info.connection_info;
  }

  static const base::TimeDelta& connection_setup_time(
      const net::ConnectionChangeNotifier::EstablishedConnectionInfo& info) {
    return info.connection_setup_time;
  }

  static net::ConnectionEstablishmentInitiator initiator(
      const net::ConnectionChangeNotifier::EstablishedConnectionInfo& info) {
    return info.initiator;
  }

  static bool Read(
      network::mojom::EstablishedConnectionInfoDataView data,
      net::ConnectionChangeNotifier::EstablishedConnectionInfo* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_CHANGE_OBSERVER_CLIENT_MOJOM_TRAITS_H_
