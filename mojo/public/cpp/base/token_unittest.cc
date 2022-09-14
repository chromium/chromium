// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/token_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/token.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {
namespace token_unittest {

TEST(TokenTest, Token) {
  base::Token in;
  base::Token out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Token>(in, out));
  EXPECT_EQ(in, out);

  constexpr uint64_t kTestHigh = 0x0123456789abcdefull;
  constexpr uint64_t kTestLow = 0x5a5a5a5aa5a5a5a5ull;
  in = base::Token{kTestHigh, kTestLow};
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Token>(in, out));
  EXPECT_EQ(in, out);

  in = base::Token::CreateRandom();
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Token>(in, out));
  EXPECT_EQ(in, out);
}

}  // namespace token_unittest
}  // namespace mojo_base
