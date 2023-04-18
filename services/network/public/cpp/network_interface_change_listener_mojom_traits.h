// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_INTERFACE_CHANGE_LISTENER_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NETWORK_INTERFACE_CHANGE_LISTENER_MOJOM_TRAITS_H_

#include <linux/rtnetlink.h>

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/address_map_linux.h"
#include "services/network/public/mojom/network_interface_change_listener.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::AddressMapDataView,
                 net::AddressMapOwnerLinux::AddressMap> {
  static const net::AddressMapOwnerLinux::AddressMap& address_map(
      const net::AddressMapOwnerLinux::AddressMap& address_map) {
    return address_map;
  }
  static bool Read(network::mojom::AddressMapDataView obj,
                   net::AddressMapOwnerLinux::AddressMap* address_map);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::OnlineLinksDataView, std::unordered_set<int>> {
  static const std::unordered_set<int>& online_links(
      const std::unordered_set<int>& online_links) {
    return online_links;
  }

  static bool Read(network::mojom::OnlineLinksDataView obj,
                   std::unordered_set<int>* online_links);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_BASE)
    StructTraits<network::mojom::IfAddrMsgDataView, struct ifaddrmsg> {
  static uint8_t ifa_family(const struct ifaddrmsg& msg) {
    return msg.ifa_family;
  }
  static uint8_t ifa_prefixlen(const struct ifaddrmsg& msg) {
    return msg.ifa_prefixlen;
  }
  static uint8_t ifa_flags(const struct ifaddrmsg& msg) {
    return msg.ifa_flags;
  }
  static uint8_t ifa_scope(const struct ifaddrmsg& msg) {
    return msg.ifa_scope;
  }
  static uint64_t ifa_index(const struct ifaddrmsg& msg) {
    return msg.ifa_index;
  }

  static bool Read(network::mojom::IfAddrMsgDataView obj,
                   struct ifaddrmsg* msg);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NETWORK_INTERFACE_CHANGE_LISTENER_MOJOM_TRAITS_H_
