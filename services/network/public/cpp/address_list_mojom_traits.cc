// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/address_list_mojom_traits.h"

#include "net/base/address_list.h"
#include "services/network/public/cpp/ip_endpoint_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::AddressListDataView, net::AddressList>::Read(
    network::mojom::AddressListDataView data,
    net::AddressList* out) {
  if (!data.ReadAddresses(&out->endpoints()))
    return false;

  std::vector<std::string> dns_aliases;
  if (!data.ReadDnsAliases(&dns_aliases))
    return false;
  out->SetDnsAliases(std::move(dns_aliases));

  return true;
}

}  // namespace mojo
