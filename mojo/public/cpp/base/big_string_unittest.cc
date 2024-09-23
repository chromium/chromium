// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/rand_util.h"
#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"
#include "mojo/public/cpp/base/big_string_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/big_string.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {
namespace big_string_unittest {

TEST(BigStringTest, Empty) {
  std::string in;
  std::string out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::BigString>(in, out));
  EXPECT_EQ(in, out);
}

TEST(BigStringTest, Short) {
  std::string in("hello world");
  std::string out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::BigString>(in, out));
  EXPECT_EQ(in, out);
}

TEST(BigStringTest, Long) {
  constexpr size_t kLargeStringSize = 1024 * 1024;

  std::string in(kLargeStringSize, 0);
  base::RandBytes(base::as_writable_byte_span(in));

  std::string out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::BigString>(in, out));
  EXPECT_EQ(in, out);
}

}  // namespace big_string_unittest
}  // namespace mojo_base
