// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ip_endpoint_mojom_traits.h"

#include "services/network/public/cpp/ip_address_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::IPEndPointDataView, net::IPEndPoint>::Read(
    network::mojom::IPEndPointDataView data,
    net::IPEndPoint* out) {
  net::IPAddress address;
  if (!data.ReadAddress(&address))
    return false;

  *out = net::IPEndPoint(address, data.port());
  return true;
}

}  // namespace mojo
