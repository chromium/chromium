// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/byte_size.h"

#include <array>
#include <limits>

#include "mojo/public/cpp/base/byte_size_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/byte_size.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {

constexpr auto kMinInt64 = std::numeric_limits<int64_t>::min();
constexpr auto kMaxInt64 = std::numeric_limits<int64_t>::max();

TEST(ByteSizeTest, ValidByteSize) {
  const auto kTestCases = std::to_array<uint64_t>({
      0u,
      1u,
      static_cast<uint64_t>(kMaxInt64) - 1,
      static_cast<uint64_t>(kMaxInt64),
  });
  for (uint64_t bytes : kTestCases) {
    SCOPED_TRACE(::testing::Message() << bytes << " bytes");
    const base::ByteSize in(bytes);
    base::ByteSize out;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ByteSize>(in, out));
    EXPECT_EQ(in, out);
  }
}

TEST(ByteSizeTest, InvalidByteSize) {
  const auto kTestCases = std::to_array<uint64_t>({
      static_cast<uint64_t>(kMaxInt64) + 1,
      std::numeric_limits<uint64_t>::max(),
  });
  for (uint64_t bytes : kTestCases) {
    SCOPED_TRACE(::testing::Message() << bytes << " bytes");
    auto in = mojom::ByteSize::New();
    in->size = bytes;
    base::ByteSize out;
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::ByteSize>(in, out));
  }
}

TEST(ByteSizeTest, ByteSizeDelta) {
  const auto kTestCases = std::to_array<int64_t>({
      kMinInt64,
      kMinInt64 + 1,
      -1,
      0,
      1,
      kMaxInt64 - 1,
      kMaxInt64,
  });
  for (int64_t bytes : kTestCases) {
    SCOPED_TRACE(::testing::Message() << bytes << " bytes");
    const base::ByteSizeDelta in(bytes);
    base::ByteSizeDelta out;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::ByteSizeDelta>(in, out));
    EXPECT_EQ(in, out);
  }
}

}  // namespace mojo_base
