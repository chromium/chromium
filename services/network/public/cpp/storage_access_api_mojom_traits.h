// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_STORAGE_ACCESS_API_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_STORAGE_ACCESS_API_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/storage_access_api/status.h"
#include "services/network/public/mojom/storage_access_api.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(STORAGE_ACCESS_API_MOJOM_TRAITS)
    EnumTraits<network::mojom::StorageAccessApiStatus,
               net::StorageAccessApiStatus> {
  static network::mojom::StorageAccessApiStatus ToMojom(
      net::StorageAccessApiStatus status);

  static bool FromMojom(network::mojom::StorageAccessApiStatus status,
                        net::StorageAccessApiStatus* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_STORAGE_ACCESS_API_MOJOM_TRAITS_H_
