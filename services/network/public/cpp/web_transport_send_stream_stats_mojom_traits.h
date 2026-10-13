// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_WEB_TRANSPORT_SEND_STREAM_STATS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_WEB_TRANSPORT_SEND_STREAM_STATS_MOJOM_TRAITS_H_

#include <cstdint>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/quic/web_transport_send_stream_stats.h"
#include "services/network/public/mojom/web_transport.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::WebTransportSendStreamStatsDataView,
                 net::WebTransportSendStreamStats> {
  static uint64_t bytes_sent(const net::WebTransportSendStreamStats& stats) {
    return stats.bytes_sent;
  }
  static uint64_t bytes_acknowledged(
      const net::WebTransportSendStreamStats& stats) {
    return stats.bytes_acknowledged;
  }
  static bool Read(network::mojom::WebTransportSendStreamStatsDataView data,
                   net::WebTransportSendStreamStats* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_WEB_TRANSPORT_SEND_STREAM_STATS_MOJOM_TRAITS_H_
