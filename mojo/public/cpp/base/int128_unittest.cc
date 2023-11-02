// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "mojo/public/cpp/base/int128_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/int128.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base::int128_unittest {

TEST(Int128Test, Int128) {
  constexpr int64_t kTestHigh = 0x0123456789abcdefll;
  constexpr uint64_t kTestLow = 0x5a5a5a5aa5a5a5a5ull;
  absl::int128 in = absl::MakeInt128(kTestHigh, kTestLow);
  absl::int128 out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Int128>(in, out));
  EXPECT_EQ(in, out);
}

TEST(Int128Test, Uint128) {
  constexpr uint64_t kTestHigh = 0x0123456789abcdefull;
  constexpr uint64_t kTestLow = 0x5a5a5a5aa5a5a5a5ull;
  absl::uint128 in = absl::MakeUint128(kTestHigh, kTestLow);
  absl::uint128 out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Uint128>(in, out));
  EXPECT_EQ(in, out);
}

}  // namespace mojo_base::int128_unittest
