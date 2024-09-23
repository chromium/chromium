// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/storage_access_api_mojom_traits.h"

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "net/storage_access_api/status.h"
#include "services/network/public/mojom/storage_access_api.mojom-shared.h"

namespace mojo {

bool EnumTraits<network::mojom::StorageAccessApiStatus,
                net::StorageAccessApiStatus>::
    FromMojom(network::mojom::StorageAccessApiStatus status,
              net::StorageAccessApiStatus* out) {
  switch (status) {
    case network::mojom::StorageAccessApiStatus::kNone:
      *out = net::StorageAccessApiStatus::kNone;
      return true;
    case network::mojom::StorageAccessApiStatus::kAccessViaAPI:
      *out = net::StorageAccessApiStatus::kAccessViaAPI;
      return true;
  }
  NOTREACHED();
}

network::mojom::StorageAccessApiStatus EnumTraits<
    network::mojom::StorageAccessApiStatus,
    net::StorageAccessApiStatus>::ToMojom(net::StorageAccessApiStatus status) {
  switch (status) {
    case net::StorageAccessApiStatus::kNone:
      return network::mojom::StorageAccessApiStatus::kNone;
    case net::StorageAccessApiStatus::kAccessViaAPI:
      return network::mojom::StorageAccessApiStatus::kAccessViaAPI;
  }
  NOTREACHED();
}

}  // namespace mojo
