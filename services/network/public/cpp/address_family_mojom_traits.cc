// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/address_family_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

// static
net::AddressFamily
EnumTraits<network::mojom::AddressFamily, net::AddressFamily>::FromMojom(
    network::mojom::AddressFamily address_family) {
  using network::mojom::AddressFamily;
  switch (address_family) {
    case AddressFamily::UNSPECIFIED:
      return net::ADDRESS_FAMILY_UNSPECIFIED;
    case AddressFamily::IPV4:
      return net::ADDRESS_FAMILY_IPV4;
    case AddressFamily::IPV6:
      return net::ADDRESS_FAMILY_IPV6;
  }
  NOTREACHED();
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
  NOTREACHED();
}

}  // namespace mojo
