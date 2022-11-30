// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiability_internal_templates.h"

#include <cstdint>
#include <limits>
#include <type_traits>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace internal {
namespace {

struct PodType {
  int x;
  float y;
  char c;
  char g[10];
};

#if !defined(ARCH_CPU_LITTLE_ENDIAN) && !defined(ARCH_CPU_BIG_ENDIAN)
#error "What kind of CPU is this?"
#endif

}  // namespace

// has_unique_object_representations
static_assert(has_unique_object_representations<int>::value, "");
static_assert(has_unique_object_representations<float>::value, "");
static_assert(has_unique_object_representations<double>::value, "");

// long double: check_blink_style doesn't let us use the word 'long' here.
static_assert(has_unique_object_representations<decltype(1.0l)>::value, "");

// Pointers aren't considered to have a unique representation.
static_assert(!has_unique_object_representations<int*>::value, "");

// Nor are POD types though they could be if they are dense and don't have any
// internal padding.
static_assert(!has_unique_object_representations<PodType>::value, "");

TEST(IdentifiabilityInternalTemplatesTest, DigestOfObjectRepresentation) {
  const int kV = 5;
  const int& kRV = kV;
  const volatile int& kRVV = kV;

  // Note that both little and big endian systems produce the same result from
  // DigestOfObjectRepresentation();

  // Positive unsigned integers.
  EXPECT_EQ(INT64_C(5), DigestOfObjectRepresentation(UINT8_C(5)));
  EXPECT_EQ(INT64_C(5), DigestOfObjectRepresentation(UINT16_C(5)));
  EXPECT_EQ(INT64_C(5), DigestOfObjectRepresentation(UINT32_C(5)));
  EXPECT_EQ(INT64_C(5), DigestOfObjectRepresentation(UINT64_C(5)));

  // Positive signed integers.
  EXPECT_EQ(INT64_C(5), DigestOfObjectRepresentation(INT8_C(5)));
  EXPECT_EQ(INT64_C(5), DigestOfObjectRepresentation(INT16_C(5)));
  EXPECT_EQ(INT64_C(5), DigestOfObjectRepresentation(INT32_C(5)));
  EXPECT_EQ(INT64_C(5), DigestOfObjectRepresentation(INT64_C(5)));
  // char
  EXPECT_EQ(INT64_C(65), DigestOfObjectRepresentation('A'));

  // Negative integers.
  EXPECT_EQ(INT64_C(-5), DigestOfObjectRepresentation(INT8_C(-5)));
  EXPECT_EQ(INT64_C(-5), DigestOfObjectRepresentation(INT16_C(-5)));
  EXPECT_EQ(INT64_C(-5), DigestOfObjectRepresentation(INT32_C(-5)));
  EXPECT_EQ(INT64_C(-5), DigestOfObjectRepresentation(INT64_C(-5)));

  // Large unsigned integer. These wrap around for 2s complement arithmetic.
  EXPECT_EQ(INT64_C(-1),
            DigestOfObjectRepresentation(std::numeric_limits<uint64_t>::max()));

  // CV qualified types should be unwrapped.
  EXPECT_EQ(INT64_C(5), DigestOfObjectRepresentation(kV));
  EXPECT_EQ(INT64_C(5), DigestOfObjectRepresentation(kRV));
  EXPECT_EQ(INT64_C(5), DigestOfObjectRepresentation(kRVV));
}

TEST(IdentifiabilityInternalTemplatesTest,
     DigestOfObjectRepresentation_Floats) {
  // IEEE 754 32-bit single precision float.
  if (sizeof(float) == 4)
    EXPECT_EQ(INT64_C(1069547520), DigestOfObjectRepresentation(1.5f));

  // IEEE 754 64-bit double precision float.
  if (sizeof(double) == 8)
    EXPECT_EQ(INT64_C(4609434218613702656), DigestOfObjectRepresentation(1.5));
}

}  // namespace internal
}  // namespace blink
