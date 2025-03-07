// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_BITSET_H_
#define SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_BITSET_H_

#include <array>
#include <cstdint>
#include <string>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

namespace network {

inline constexpr size_t kPermissionsPolicyFeaturesBitsetSize =
    static_cast<size_t>(network::mojom::PermissionsPolicyFeature::kMaxValue) +
    1;
inline constexpr size_t kPermissionsPolicyFeaturesBitsetArraySize =
    1 + (kPermissionsPolicyFeaturesBitsetSize - 1) / 8;

// A custom bitset class used to keep track of permissions policy feature state
// across processes. `std::bitset<>` and `base::EnumSet<>` are intentionally not
// used because they do not provide the flexibility needed to efficiently
// serialize/deserialize the set.
class COMPONENT_EXPORT(NETWORK_CPP_WEB_PLATFORM)
    PermissionsPolicyFeaturesBitset {
 public:
  // Creates an empty PermissionsPolicyFeaturesBitset with the size of
  // `network::mojom::PermissionsPolicyFeature`.
  explicit PermissionsPolicyFeaturesBitset();

  PermissionsPolicyFeaturesBitset(const PermissionsPolicyFeaturesBitset&);
  PermissionsPolicyFeaturesBitset& operator=(
      const PermissionsPolicyFeaturesBitset&);

  PermissionsPolicyFeaturesBitset(PermissionsPolicyFeaturesBitset&&);
  PermissionsPolicyFeaturesBitset& operator=(PermissionsPolicyFeaturesBitset&&);

  friend bool operator==(const PermissionsPolicyFeaturesBitset&,
                         const PermissionsPolicyFeaturesBitset&) = default;

  ~PermissionsPolicyFeaturesBitset();

  // Adds the given `feature` to the bitset.
  void Add(network::mojom::PermissionsPolicyFeature feature);
  // Returns whether the given `feature` is present in the bitset.
  bool Contains(network::mojom::PermissionsPolicyFeature feature) const;

  // Serializes `this` and returns the compressed value. The bitset can be
  // deserialized with `Deserialize()`.
  std::string Serialize() const;

  // Deserializes the bitset from `data`. If `data` is longer than the bitset,
  // returns false.
  bool Deserialize(std::string_view data);

  // Size of the bitset as number of bits.
  size_t bitset_size() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(PermissionsPolicyFeaturesBitsetTest,
                           PermissionsPolicyFeaturesBitset);
  FRIEND_TEST_ALL_PREFIXES(PermissionsPolicyFeaturesBitsetTest,
                           DeserializePartialData);
  FRIEND_TEST_ALL_PREFIXES(PermissionsPolicyFeaturesBitsetTest,
                           DeserializeRepeatedly);

  // Returns whether the given `index` is present in the bitset. Carved out of
  // the public `Contains()` method to make it available for tests because the
  // enums are not sequential.
  bool Contains(size_t index) const;
  // Returns which element `index` maps to in `bitset_`.
  size_t ToInternalIndex(size_t index) const;
  // Returns which bit `index` maps to given a certain `bitset_` element.
  uint8_t ToBitmask(size_t index) const;

  std::array<uint8_t, kPermissionsPolicyFeaturesBitsetArraySize> bitset_ = {};
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_PERMISSIONS_POLICY_PERMISSIONS_POLICY_FEATURES_BITSET_H_
