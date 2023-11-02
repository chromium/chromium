// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

#include <functional>
#include <unordered_set>

#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// These metric names were chosen so that they result in a surface type of
// kReservedInternal. These are static_asserts because these expressions should
// resolve at compile-time.
static_assert(IdentifiableSurface::FromMetricHash(
                  ukm::builders::Identifiability::kStudyGeneration_626NameHash)
                      .GetType() ==
                  IdentifiableSurface::Type::kReservedInternal,
              "");
static_assert(IdentifiableSurface::FromMetricHash(
                  ukm::builders::Identifiability::kGeneratorVersion_926NameHash)
                      .GetType() ==
                  IdentifiableSurface::Type::kReservedInternal,
              "");

TEST(IdentifiableSurfaceTest, FromTypeAndTokenIsConstexpr) {
  constexpr uint64_t kTestInputHash = 5u;
  constexpr auto kSurface = IdentifiableSurface::FromTypeAndToken(
      IdentifiableSurface::Type::kWebFeature, kTestInputHash);

  static_assert(
      (kTestInputHash << 8) +
              static_cast<uint64_t>(IdentifiableSurface::Type::kWebFeature) ==
          kSurface.ToUkmMetricHash(),
      "");
  static_assert(IdentifiableSurface::Type::kWebFeature == kSurface.GetType(),
                "");
  static_assert(kTestInputHash == kSurface.GetInputHash(), "");
}

TEST(IdentifiableSurfaceTest, FromKeyIsConstexpr) {
  constexpr uint64_t kTestInputHash = 5u;
  constexpr uint64_t kTestMetricHash =
      ((kTestInputHash << 8) |
       static_cast<uint64_t>(IdentifiableSurface::Type::kWebFeature));
  constexpr auto kSurface =
      IdentifiableSurface::FromMetricHash(kTestMetricHash);
  static_assert(kTestMetricHash == kSurface.ToUkmMetricHash(), "");
  static_assert(IdentifiableSurface::Type::kWebFeature == kSurface.GetType(),
                "");
}

TEST(IdentifiableSurfaceTest, AllowsMaxTypeValue) {
  constexpr uint64_t kInputHash = UINT64_C(0x1123456789abcdef);
  constexpr auto kSurface = IdentifiableSurface::FromTypeAndToken(
      IdentifiableSurface::Type::kMax, kInputHash);

  EXPECT_EQ(UINT64_C(0x23456789abcdefff), kSurface.ToUkmMetricHash());
  EXPECT_EQ(IdentifiableSurface::Type::kMax, kSurface.GetType());

  // The lower 56 bits of kInputHash should match GetInputHash().
  EXPECT_EQ(kInputHash << 8, kSurface.GetInputHash() << 8);
  EXPECT_NE(kInputHash, kSurface.GetInputHash());
}

TEST(IdentifiableSurfaceTest, IdentifiableSurfaceHash) {
  constexpr uint64_t kTestInputHashA = 1;
  constexpr uint64_t kTestInputHashB = 3;

  // surface2 == surface3 != surface1
  auto surface1 = IdentifiableSurface::FromTypeAndToken(
      IdentifiableSurface::Type::kWebFeature, kTestInputHashA);
  auto surface2 = IdentifiableSurface::FromTypeAndToken(
      IdentifiableSurface::Type::kWebFeature, kTestInputHashB);
  auto surface3 = IdentifiableSurface::FromTypeAndToken(
      IdentifiableSurface::Type::kWebFeature, kTestInputHashB);

  IdentifiableSurfaceHash hash_object;

  size_t hash1 = hash_object(surface1);
  size_t hash2 = hash_object(surface2);
  size_t hash3 = hash_object(surface3);

  EXPECT_NE(hash1, hash2);
  EXPECT_EQ(hash3, hash2);

  std::unordered_set<IdentifiableSurface, IdentifiableSurfaceHash> surface_set;
  surface_set.insert(surface1);
  surface_set.insert(surface2);
  surface_set.insert(surface3);

  EXPECT_EQ(surface_set.size(), 2u);
  EXPECT_EQ(surface_set.count(surface1), 1u);
}

TEST(IdentifiableSurfaceTest, Comparison) {
  constexpr uint64_t kTestInputHashA = 1;
  constexpr uint64_t kTestInputHashB = 3;

  // surface2 == surface3 != surface1
  auto surface1 = IdentifiableSurface::FromTypeAndToken(
      IdentifiableSurface::Type::kWebFeature, kTestInputHashA);
  auto surface2 = IdentifiableSurface::FromTypeAndToken(
      IdentifiableSurface::Type::kWebFeature, kTestInputHashB);
  auto surface3 = IdentifiableSurface::FromTypeAndToken(
      IdentifiableSurface::Type::kWebFeature, kTestInputHashB);

  EXPECT_TRUE(surface2 == surface3);
  EXPECT_TRUE(surface1 != surface3);
  EXPECT_TRUE(surface1 < surface2);
}

}  // namespace blink
