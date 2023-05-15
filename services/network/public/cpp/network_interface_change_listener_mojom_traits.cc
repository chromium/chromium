// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_interface_change_listener_mojom_traits.h"

#include <unordered_set>

#include "mojo/public/cpp/bindings/array_traits_stl.h"
#include "net/base/address_map_linux.h"
#include "services/network/public/cpp/ip_address_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::AddressMapDataView,
                  net::AddressMapOwnerLinux::AddressMap>::
    Read(network::mojom::AddressMapDataView obj,
         net::AddressMapOwnerLinux::AddressMap* address_map) {
  return obj.ReadAddressMap(address_map);
}

// static
bool StructTraits<
    network::mojom::OnlineLinksDataView,
    std::unordered_set<int>>::Read(network::mojom::OnlineLinksDataView obj,
                                   std::unordered_set<int>* online_links) {
  mojo::ArrayDataView<int32_t> online_links_data_view;
  obj.GetOnlineLinksDataView(&online_links_data_view);
  online_links->reserve(online_links_data_view.size());
  for (size_t i = 0; i < online_links_data_view.size(); i++) {
    auto ret = online_links->insert(online_links_data_view[i]);
    if (!ret.second) {
      // There was a duplicate link value in the array, but it's supposed to be
      // a set.
      return false;
    }
  }
  return true;
}

// static
bool StructTraits<network::mojom::IfAddrMsgDataView, struct ifaddrmsg>::Read(
    network::mojom::IfAddrMsgDataView obj,
    struct ifaddrmsg* msg) {
  msg->ifa_family = obj.ifa_family();
  msg->ifa_prefixlen = obj.ifa_prefixlen();
  msg->ifa_flags = obj.ifa_flags();
  msg->ifa_scope = obj.ifa_scope();
  msg->ifa_index = obj.ifa_index();
  return true;
}

}  // namespace mojo
