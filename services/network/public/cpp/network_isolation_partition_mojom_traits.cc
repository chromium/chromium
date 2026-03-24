// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_isolation_partition_mojom_traits.h"

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/base/network_isolation_partition.h"
#include "services/network/public/mojom/network_isolation_partition.mojom.h"

namespace mojo {

net::NetworkIsolationPartition
EnumTraits<network::mojom::NetworkIsolationPartition,
           net::NetworkIsolationPartition>::
    FromMojom(
        network::mojom::NetworkIsolationPartition network_isolation_partition) {
  using network::mojom::NetworkIsolationPartition;
  switch (network_isolation_partition) {
    case NetworkIsolationPartition::kGeneral:
      return net::NetworkIsolationPartition::kGeneral;
    case NetworkIsolationPartition::kProtectedAudienceSellerWorklet:
      return net::NetworkIsolationPartition::kProtectedAudienceSellerWorklet;
    case NetworkIsolationPartition::kFedCmUncredentialedRequests:
      return net::NetworkIsolationPartition::kFedCmUncredentialedRequests;
    case NetworkIsolationPartition::kDnsOverHttps:
      return net::NetworkIsolationPartition::kDnsOverHttps;
  }
  NOTREACHED();
}

// static
network::mojom::NetworkIsolationPartition EnumTraits<
    network::mojom::NetworkIsolationPartition,
    net::NetworkIsolationPartition>::ToMojom(net::NetworkIsolationPartition
                                                 network_isolation_partition) {
  using network::mojom::NetworkIsolationPartition;
  switch (network_isolation_partition) {
    case net::NetworkIsolationPartition::kGeneral:
      return NetworkIsolationPartition::kGeneral;
    case net::NetworkIsolationPartition::kProtectedAudienceSellerWorklet:
      return NetworkIsolationPartition::kProtectedAudienceSellerWorklet;
    case net::NetworkIsolationPartition::kFedCmUncredentialedRequests:
      return NetworkIsolationPartition::kFedCmUncredentialedRequests;
    case net::NetworkIsolationPartition::kDnsOverHttps:
      return NetworkIsolationPartition::kDnsOverHttps;
  }
  NOTREACHED();
}

}  // namespace mojo
