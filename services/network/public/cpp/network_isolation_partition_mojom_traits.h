// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_ISOLATION_PARTITION_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NETWORK_ISOLATION_PARTITION_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/base/network_isolation_partition.h"
#include "services/network/public/mojom/network_isolation_partition.mojom.h"

namespace mojo {

template <>
struct EnumTraits<network::mojom::NetworkIsolationPartition,
                  net::NetworkIsolationPartition> {
  static network::mojom::NetworkIsolationPartition ToMojom(
      net::NetworkIsolationPartition network_isolation_partition);
  static bool FromMojom(
      network::mojom::NetworkIsolationPartition network_isolation_partition,
      net::NetworkIsolationPartition* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NETWORK_ISOLATION_PARTITION_MOJOM_TRAITS_H_
