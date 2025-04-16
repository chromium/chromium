// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_RECONNECT_EVENT_OBSERVER_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_RECONNECT_EVENT_OBSERVER_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/reconnect_notifier.h"
#include "services/network/public/mojom/reconnect_event_observer.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<network::mojom::ConnectionKeepAliveConfigDataView,
                    net::ConnectionKeepAliveConfig> {
 public:
  static bool idle_timeout_in_seconds(
      const net::ConnectionKeepAliveConfig& keep_alive_config) {
    return keep_alive_config.idle_timeout_in_seconds;
  }

  static bool ping_interval_in_seconds(
      const net::ConnectionKeepAliveConfig& keep_alive_config) {
    return keep_alive_config.ping_interval_in_seconds;
  }

  static bool Read(network::mojom::ConnectionKeepAliveConfigDataView data,
                   net::ConnectionKeepAliveConfig* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_RECONNECT_EVENT_OBSERVER_MOJOM_TRAITS_H_
