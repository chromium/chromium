// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/integrity_metadata_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<network::mojom::IntegrityMetadataDataView,
                  network::IntegrityMetadata>::
    Read(network::mojom::IntegrityMetadataDataView data,
         network::IntegrityMetadata* out) {
  network::mojom::IntegrityAlgorithm algorithm;
  std::vector<uint8_t> value;
  if (!data.ReadAlgorithm(&algorithm) || !data.ReadValue(&value)) {
    return false;
  }
  *out = network::IntegrityMetadata(algorithm, std::move(value));
  return true;
}

}  // namespace mojo
