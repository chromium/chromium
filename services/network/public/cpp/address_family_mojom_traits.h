// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ADDRESS_FAMILY_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ADDRESS_FAMILY_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/base/address_family.h"
#include "services/network/public/mojom/address_family.mojom.h"

namespace mojo {

template <>
struct EnumTraits<network::mojom::AddressFamily, net::AddressFamily> {
  static network::mojom::AddressFamily ToMojom(
      net::AddressFamily address_family);
  static bool FromMojom(network::mojom::AddressFamily address_family,
                        net::AddressFamily* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_ADDRESS_FAMILY_MOJOM_TRAITS_H_
