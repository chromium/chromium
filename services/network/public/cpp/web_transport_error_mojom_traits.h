// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_WEB_TRANSPORT_ERROR_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_WEB_TRANSPORT_ERROR_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/quic/web_transport_error.h"
#include "services/network/public/mojom/web_transport.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::WebTransportErrorDataView,
                 net::WebTransportError> {
  static int32_t net_error(const net::WebTransportError& e) {
    return e.net_error;
  }
  static int32_t quic_error(const net::WebTransportError& e) {
    return static_cast<int32_t>(e.quic_error);
  }
  static const std::string& details(const net::WebTransportError& e) {
    return e.details;
  }
  static bool safe_to_report_details(const net::WebTransportError& e) {
    return e.safe_to_report_details;
  }
  static bool Read(network::mojom::WebTransportErrorDataView in,
                   net::WebTransportError* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_WEB_TRANSPORT_ERROR_MOJOM_TRAITS_H_
