// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/web_transport_send_stream_stats_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::WebTransportSendStreamStatsDataView,
                  net::WebTransportSendStreamStats>::
    Read(network::mojom::WebTransportSendStreamStatsDataView data,
         net::WebTransportSendStreamStats* out) {
  out->bytes_sent = data.bytes_sent();
  out->bytes_acknowledged = data.bytes_acknowledged();
  return true;
}

}  // namespace mojo
