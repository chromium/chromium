// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/permissions_policy/permissions_policy_features_bitset.h"

#include <cstring>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

namespace network {

PermissionsPolicyFeaturesBitset::PermissionsPolicyFeaturesBitset(
    const PermissionsPolicyFeaturesBitset&) = default;
PermissionsPolicyFeaturesBitset& PermissionsPolicyFeaturesBitset::operator=(
    const PermissionsPolicyFeaturesBitset&) = default;

PermissionsPolicyFeaturesBitset::PermissionsPolicyFeaturesBitset(
    PermissionsPolicyFeaturesBitset&&) = default;
PermissionsPolicyFeaturesBitset& PermissionsPolicyFeaturesBitset::operator=(
    PermissionsPolicyFeaturesBitset&&) = default;

PermissionsPolicyFeaturesBitset::PermissionsPolicyFeaturesBitset() = default;

PermissionsPolicyFeaturesBitset::~PermissionsPolicyFeaturesBitset() = default;

void PermissionsPolicyFeaturesBitset::Add(
    network::mojom::PermissionsPolicyFeature feature) {
  size_t index = static_cast<size_t>(feature);
  CHECK_LT(index, kPermissionsPolicyFeaturesBitsetSize);
  size_t internal_index = ToInternalIndex(index);
  uint8_t bitmask = ToBitmask(index);
  bitset_[internal_index] |= bitmask;
}

bool PermissionsPolicyFeaturesBitset::Contains(
    network::mojom::PermissionsPolicyFeature feature) const {
  return Contains(static_cast<size_t>(feature));
}

bool PermissionsPolicyFeaturesBitset::Contains(size_t index) const {
  CHECK_LT(index, kPermissionsPolicyFeaturesBitsetSize);
  size_t internal_index = ToInternalIndex(index);
  uint8_t bitmask = ToBitmask(index);
  return (bitset_[internal_index] & bitmask) != 0;
}

std::string PermissionsPolicyFeaturesBitset::Serialize() const {
  // Since the bitset is stored from right to left, as an optimization, omit all
  // the leftmost 0's.
  size_t offset;
  for (offset = 0; offset < bitset_.size(); ++offset) {
    if (bitset_[offset] != 0) {
      break;
    }
  }

  base::span<const char> s =
      base::as_chars(base::span(bitset_).subspan(offset));
  return std::string(s.begin(), s.end());
}

bool PermissionsPolicyFeaturesBitset::Deserialize(std::string_view data) {
  if (data.size() > bitset_.size()) {
    return false;
  }

  // Copy the passed `data` to the end of the internal `bitset_`. For example,
  // if `data` is {0xAA, 0xBB}, and set size is 32 (so `bitset_` is a vector of
  // 4 uint8_t's), then the final `bitset_` should be {0x00, 0x00, 0xAA, 0xBB}.
  base::span(bitset_).last(data.size()).copy_from(base::as_byte_span((data)));

  // Zero out the rest of `bitset_` to clear any residual data.
  size_t zero_count = kPermissionsPolicyFeaturesBitsetArraySize - data.size();
  if (zero_count > 0) {
    std::fill(bitset_.begin(), bitset_.begin() + zero_count, 0);
  }

  return true;
}

size_t PermissionsPolicyFeaturesBitset::bitset_size() const {
  return kPermissionsPolicyFeaturesBitsetSize;
}

size_t PermissionsPolicyFeaturesBitset::ToInternalIndex(size_t index) const {
  // Note: internally, the bitset is stored from right to left. For example,
  // index 0 maps to the least significant bit of the last element of `bitset_`.
  return bitset_.size() - 1 - index / 8;
}

uint8_t PermissionsPolicyFeaturesBitset::ToBitmask(size_t index) const {
  return 1 << (index % 8);
}

}  // namespace network
