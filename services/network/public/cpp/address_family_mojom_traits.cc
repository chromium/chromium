// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/address_family_mojom_traits.h"

namespace mojo {

// static
bool EnumTraits<network::mojom::AddressFamily, net::AddressFamily>::FromMojom(
    network::mojom::AddressFamily address_family,
    net::AddressFamily* out) {
  using network::mojom::AddressFamily;
  switch (address_family) {
    case AddressFamily::UNSPECIFIED:
      *out = net::ADDRESS_FAMILY_UNSPECIFIED;
      return true;
    case AddressFamily::IPV4:
      *out = net::ADDRESS_FAMILY_IPV4;
      return true;
    case AddressFamily::IPV6:
      *out = net::ADDRESS_FAMILY_IPV6;
      return true;
  }
  return false;
}

// static
network::mojom::AddressFamily
EnumTraits<network::mojom::AddressFamily, net::AddressFamily>::ToMojom(
    net::AddressFamily address_family) {
  using network::mojom::AddressFamily;
  switch (address_family) {
    case net::ADDRESS_FAMILY_UNSPECIFIED:
      return AddressFamily::UNSPECIFIED;
    case net::ADDRESS_FAMILY_IPV4:
      return AddressFamily::IPV4;
    case net::ADDRESS_FAMILY_IPV6:
      return AddressFamily::IPV6;
  }
  NOTREACHED_IN_MIGRATION();
  return AddressFamily::UNSPECIFIED;
}

}  // namespace mojo
