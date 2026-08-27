// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_limit.h"

#include <array>
#include <limits>

#include "mojo/public/cpp/base/memory_limit_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/memory_limit.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {

TEST(MemoryLimitTest, ValidMemoryLimit) {
  const auto kTestCases = std::to_array<int>({
      0,
      1,
      50,
      100,
      150,
      std::numeric_limits<int>::max(),
  });
  for (int percent : kTestCases) {
    SCOPED_TRACE(::testing::Message() << percent << "%");
    const base::MemoryLimit in = base::MemoryLimit::FromPercent(percent);
    base::MemoryLimit out;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::MemoryLimit>(in, out));
    EXPECT_EQ(in, out);
  }
}

TEST(MemoryLimitTest, InvalidMemoryLimit) {
  const auto kTestCases = std::to_array<int>({
      -1,
      -50,
      -100,
      std::numeric_limits<int>::min(),
  });
  for (int percent : kTestCases) {
    SCOPED_TRACE(::testing::Message() << percent << "%");
    auto in = mojom::MemoryLimit::New();
    in->percentage = percent;
    base::MemoryLimit out;
    EXPECT_FALSE(
        mojo::test::SerializeAndDeserialize<mojom::MemoryLimit>(in, out));
  }
}

}  // namespace mojo_base
