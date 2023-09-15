// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/web_transport_error_mojom_traits.h"

namespace mojo {

bool StructTraits<
    network::mojom::WebTransportErrorDataView,
    net::WebTransportError>::Read(network::mojom::WebTransportErrorDataView in,
                                  net::WebTransportError* out) {
  if (in.net_error() >= 0) {
    return false;
  }
  if (in.quic_error() < 0 ||
      in.quic_error() >= static_cast<int>(quic::QUIC_LAST_ERROR)) {
    return false;
  }
  std::string details;
  if (!in.ReadDetails(&details)) {
    return false;
  }

  *out = net::WebTransportError(
      in.net_error(), static_cast<quic::QuicErrorCode>(in.quic_error()),
      std::move(details), in.safe_to_report_details());
  return true;
}

}  // namespace mojo
