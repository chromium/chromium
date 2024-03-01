// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ip_address_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::IPAddressDataView, net::IPAddress>::Read(
    network::mojom::IPAddressDataView data,
    net::IPAddress* out) {
  std::vector<uint8_t> bytes;
  if (!data.ReadAddressBytes(&bytes))
    return false;

  if (bytes.size() && bytes.size() != net::IPAddress::kIPv4AddressSize &&
      bytes.size() != net::IPAddress::kIPv6AddressSize) {
    return false;
  }

  *out = net::IPAddress(bytes);
  return true;
}

}  // namespace mojo
