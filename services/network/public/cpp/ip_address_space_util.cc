// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ip_address_space_util.h"

#include "net/base/ip_address.h"

namespace network {

using mojom::IPAddressSpace;

IPAddressSpace IPAddressToIPAddressSpace(const net::IPAddress& address) {
  if (!address.IsValid()) {
    return IPAddressSpace::kUnknown;
  }

  if (address.IsLoopback()) {
    return IPAddressSpace::kLocal;
  }

  if (!address.IsPubliclyRoutable()) {
    return IPAddressSpace::kPrivate;
  }

  return IPAddressSpace::kPublic;
}

// For comparison purposes, we treat kUnknown the same as kPublic.
IPAddressSpace CollapseUnknown(IPAddressSpace space) {
  if (space == IPAddressSpace::kUnknown) {
    return IPAddressSpace::kPublic;
  }
  return space;
}

bool IsLessPublicAddressSpace(IPAddressSpace lhs, IPAddressSpace rhs) {
  // Apart from the special case for kUnknown, the built-in comparison operator
  // works just fine. The comment on IPAddressSpace's definition notes that the
  // enum values' ordering matters.
  return CollapseUnknown(lhs) < CollapseUnknown(rhs);
}

}  // namespace network
