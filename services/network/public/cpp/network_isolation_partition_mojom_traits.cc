// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_isolation_partition_mojom_traits.h"

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/base/network_isolation_partition.h"
#include "services/network/public/mojom/network_isolation_partition.mojom.h"

namespace mojo {

bool EnumTraits<network::mojom::NetworkIsolationPartition,
                net::NetworkIsolationPartition>::
    FromMojom(
        network::mojom::NetworkIsolationPartition network_isolation_partition,
        net::NetworkIsolationPartition* out) {
  using network::mojom::NetworkIsolationPartition;
  switch (network_isolation_partition) {
    case NetworkIsolationPartition::kGeneral:
      *out = net::NetworkIsolationPartition::kGeneral;
      return true;
    case NetworkIsolationPartition::kProtectedAudienceSellerWorklet:
      *out = net::NetworkIsolationPartition::kProtectedAudienceSellerWorklet;
      return true;
    case NetworkIsolationPartition::kFedCmUncredentialedRequests:
      *out = net::NetworkIsolationPartition::kFedCmUncredentialedRequests;
      return true;
  }
  return false;
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
  }
  NOTREACHED();
}

}  // namespace mojo
