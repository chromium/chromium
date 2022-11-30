// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/unguessable_token.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {
namespace unguessable_token_unittest {

TEST(UnguessableTokenTest, UnguessableToken) {
  base::UnguessableToken in = base::UnguessableToken::Create();
  base::UnguessableToken out;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::UnguessableToken>(in, out));
  EXPECT_EQ(in, out);
}

}  // namespace unguessable_token_unittest
}  // namespace mojo_base
