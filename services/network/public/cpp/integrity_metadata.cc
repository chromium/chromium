// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/integrity_metadata.h"

#include "base/base64.h"

namespace network {

IntegrityMetadata::IntegrityMetadata() = default;
IntegrityMetadata::~IntegrityMetadata() = default;

IntegrityMetadata::IntegrityMetadata(mojom::IntegrityAlgorithm algorithm,
                                     std::vector<uint8_t> value)
    : algorithm(algorithm), value(std::move(value)) {}
IntegrityMetadata::IntegrityMetadata(mojom::IntegrityAlgorithm algorithm,
                                     base::span<const uint8_t> value)
    : algorithm(algorithm), value(value.begin(), value.end()) {}

IntegrityMetadata::IntegrityMetadata(const IntegrityMetadata&) = default;
IntegrityMetadata& IntegrityMetadata::operator=(const IntegrityMetadata&) =
    default;
IntegrityMetadata::IntegrityMetadata(IntegrityMetadata&&) = default;
IntegrityMetadata& IntegrityMetadata::operator=(IntegrityMetadata&&) = default;

// static
std::optional<IntegrityMetadata> IntegrityMetadata::CreateFromBase64(
    mojom::IntegrityAlgorithm algorithm,
    std::string_view base64_encoded_value) {
  std::optional<std::vector<uint8_t>> decoded =
      base::Base64Decode(base64_encoded_value);
  if (!decoded) {
    return std::nullopt;
  }
  return IntegrityMetadata(algorithm, std::move(*decoded));
}

}  // namespace network
