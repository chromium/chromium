// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_INTEGRITY_METADATA_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_INTEGRITY_METADATA_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/containers/span.h"
#include "services/network/public/cpp/integrity_metadata.h"
#include "services/network/public/mojom/integrity_metadata.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_INTEGRITY_METADATA)
    StructTraits<network::mojom::IntegrityMetadataDataView,
                 network::IntegrityMetadata> {
  static network::mojom::IntegrityAlgorithm algorithm(
      const network::IntegrityMetadata& r) {
    return r.algorithm;
  }

  static base::span<const uint8_t> value(const network::IntegrityMetadata& r) {
    return r.value;
  }

  static bool Read(network::mojom::IntegrityMetadataDataView data,
                   network::IntegrityMetadata* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_INTEGRITY_METADATA_MOJOM_TRAITS_H_
