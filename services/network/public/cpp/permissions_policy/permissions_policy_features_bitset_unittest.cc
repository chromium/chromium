// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/permissions_policy/permissions_policy_features_bitset.h"

#include <array>

#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(PermissionsPolicyFeaturesBitsetTest, PermissionsPolicyFeaturesBitset) {
  // Create a bitset and verify that it is initially empty.
  PermissionsPolicyFeaturesBitset bitset1;
  EXPECT_EQ(kPermissionsPolicyFeaturesBitsetSize, bitset1.bitset_size());
  for (size_t i = 0; i < bitset1.bitset_size(); ++i) {
    EXPECT_FALSE(bitset1.Contains(i));
  }
  EXPECT_TRUE(bitset1.Serialize().empty());

  // Add some elements to the bitset, and verify they can be looked up.
  bitset1.Add(network::mojom::PermissionsPolicyFeature::kNotFound);
  bitset1.Add(network::mojom::PermissionsPolicyFeature::kUsb);
  for (size_t i = 0; i < bitset1.bitset_size(); ++i) {
    EXPECT_EQ(bitset1.Contains(i), i == 0 || i == 14);
  }

  // Serialize the bitset. The trailing zeros should be optimized away, and
  // the resulting output should fit in 2 bytes (0b01000000 and 0b00000001).
  std::string serialized = bitset1.Serialize();
  EXPECT_EQ(serialized.size(), 2U);
  EXPECT_EQ(static_cast<uint8_t>(serialized[0]), 0b01000000);
  EXPECT_EQ(static_cast<uint8_t>(serialized[1]), 0b00000001);

  // Create a new bitset using the serialized data and verify that it contains
  // the same data.
  PermissionsPolicyFeaturesBitset bitset2;
  EXPECT_TRUE(bitset2.Deserialize(serialized));
  EXPECT_EQ(kPermissionsPolicyFeaturesBitsetSize, bitset2.bitset_size());
  for (size_t i = 0; i < bitset2.bitset_size(); ++i) {
    EXPECT_EQ(bitset1.Contains(i), bitset2.Contains(i));
  }

  // Do some last few checks for good measure.
  bitset2.Add(network::mojom::PermissionsPolicyFeature::kScreenWakeLock);
  // Adding the same element a second time should have no impact.
  bitset2.Add(network::mojom::PermissionsPolicyFeature::kScreenWakeLock);
  for (size_t i = 0; i < bitset2.bitset_size(); ++i) {
    EXPECT_EQ(bitset2.Contains(i), i == 0 || i == 14 || i == 31);
  }
  serialized = bitset2.Serialize();
  EXPECT_EQ(serialized.size(), 4U);
  EXPECT_EQ(static_cast<uint8_t>(serialized[0]), 0b10000000);
  EXPECT_EQ(static_cast<uint8_t>(serialized[1]), 0b00000000);
  EXPECT_EQ(static_cast<uint8_t>(serialized[2]), 0b01000000);
  EXPECT_EQ(static_cast<uint8_t>(serialized[3]), 0b00000001);
}

TEST(PermissionsPolicyFeaturesBitsetTest, DeserializeTooLargeData) {
  std::string data(kPermissionsPolicyFeaturesBitsetArraySize + 1, 0xFF);
  PermissionsPolicyFeaturesBitset bitset;
  EXPECT_FALSE(bitset.Deserialize(data));
}

TEST(PermissionsPolicyFeaturesBitsetTest, DeserializePartialData) {
  PermissionsPolicyFeaturesBitset bitset;
  std::string data = "\x01\x02\x03";
  ASSERT_TRUE(bitset.Deserialize(data));

  std::array<uint8_t, kPermissionsPolicyFeaturesBitsetArraySize>
      expected_bitset = {};
  expected_bitset[kPermissionsPolicyFeaturesBitsetArraySize - 3] = 0x01;
  expected_bitset[kPermissionsPolicyFeaturesBitsetArraySize - 2] = 0x02;
  expected_bitset[kPermissionsPolicyFeaturesBitsetArraySize - 1] = 0x03;

  EXPECT_EQ(bitset.bitset_, expected_bitset);
}

TEST(PermissionsPolicyFeaturesBitsetTest, DeserializeRepeatedly) {
  PermissionsPolicyFeaturesBitset bitset;
  std::string data1 = "\xAA\xBB";
  ASSERT_TRUE(bitset.Deserialize(data1));

  std::array<uint8_t, kPermissionsPolicyFeaturesBitsetArraySize>
      expected_bitset1 = {};
  expected_bitset1[kPermissionsPolicyFeaturesBitsetArraySize - 2] = 0xAA;
  expected_bitset1[kPermissionsPolicyFeaturesBitsetArraySize - 1] = 0xBB;

  ASSERT_EQ(bitset.bitset_, expected_bitset1);

  std::string data2 = "\xCC";
  ASSERT_TRUE(bitset.Deserialize(data2));

  std::array<uint8_t, kPermissionsPolicyFeaturesBitsetArraySize>
      expected_bitset2 = {};
  expected_bitset2[kPermissionsPolicyFeaturesBitsetArraySize - 1] = 0xCC;

  ASSERT_EQ(bitset.bitset_, expected_bitset2);
}

}  // namespace network
