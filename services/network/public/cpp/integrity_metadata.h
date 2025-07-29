// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_INTEGRITY_METADATA_H_
#define SERVICES_NETWORK_PUBLIC_CPP_INTEGRITY_METADATA_H_

#include <compare>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "services/network/public/mojom/integrity_algorithm.mojom.h"

namespace network {

// Typemapped to `network::mojom::IntegrityMetadata`:
struct COMPONENT_EXPORT(NETWORK_CPP_INTEGRITY_METADATA) IntegrityMetadata {
  IntegrityMetadata();
  ~IntegrityMetadata();
  IntegrityMetadata(mojom::IntegrityAlgorithm algorithm,
                    std::vector<uint8_t> value);
  IntegrityMetadata(mojom::IntegrityAlgorithm algorithm,
                    base::span<const uint8_t> value);
  IntegrityMetadata(const IntegrityMetadata&);
  IntegrityMetadata& operator=(const IntegrityMetadata&);
  IntegrityMetadata(IntegrityMetadata&&);
  IntegrityMetadata& operator=(IntegrityMetadata&&);

  auto operator<=>(const IntegrityMetadata&) const = default;
  bool operator==(const IntegrityMetadata&) const = default;

  static std::optional<IntegrityMetadata> CreateFromBase64(
      mojom::IntegrityAlgorithm algorithm,
      std::string_view base64_encoded_value);

  mojom::IntegrityAlgorithm algorithm;
  std::vector<uint8_t> value;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_INTEGRITY_METADATA_H_
