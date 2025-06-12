// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_CHANGE_OBSERVER_CLIENT_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_CHANGE_OBSERVER_CLIENT_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/reconnect_notifier.h"
#include "services/network/public/mojom/connection_change_observer_client.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<network::mojom::NetworkChangeEvent, net::NetworkChangeEvent> {
  static network::mojom::NetworkChangeEvent ToMojom(
      net::NetworkChangeEvent event_type);

  static bool FromMojom(network::mojom::NetworkChangeEvent event_type,
                        net::NetworkChangeEvent* out);
};

template <>
struct StructTraits<network::mojom::ConnectionKeepAliveConfigDataView,
                    net::ConnectionKeepAliveConfig> {
 public:
  static int32_t idle_timeout_in_seconds(
      const net::ConnectionKeepAliveConfig& keep_alive_config) {
    return keep_alive_config.idle_timeout_in_seconds;
  }

  static int32_t ping_interval_in_seconds(
      const net::ConnectionKeepAliveConfig& keep_alive_config) {
    return keep_alive_config.ping_interval_in_seconds;
  }

  static bool enable_connection_keep_alive(
      const net::ConnectionKeepAliveConfig& keep_alive_config) {
    return keep_alive_config.enable_connection_keep_alive;
  }

  static std::string quic_connection_options(
      const net::ConnectionKeepAliveConfig& keep_alive_config) {
    return keep_alive_config.quic_connection_options;
  }

  static bool Read(network::mojom::ConnectionKeepAliveConfigDataView data,
                   net::ConnectionKeepAliveConfig* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_CHANGE_OBSERVER_CLIENT_MOJOM_TRAITS_H_
