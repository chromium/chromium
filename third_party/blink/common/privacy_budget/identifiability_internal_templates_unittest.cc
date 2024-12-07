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

}  // namespace internal
}  // namespace blink
