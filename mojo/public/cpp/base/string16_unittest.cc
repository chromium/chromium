// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string16.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/string16.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {
namespace string16_unittest {

TEST(String16Test, Empty) {
  base::string16 in;
  base::string16 out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::String16>(in, out));
  EXPECT_EQ(in, out);
}

TEST(String16Test, NonEmpty) {
  base::string16 in = base::ASCIIToUTF16("hello world");
  base::string16 out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::String16>(in, out));
  EXPECT_EQ(in, out);
}

TEST(BigString16Test, Empty) {
  base::string16 in;
  base::string16 out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::BigString16>(in, out));
  EXPECT_EQ(in, out);
}

TEST(BigString16Test, Short) {
  base::string16 in = base::ASCIIToUTF16("hello world");
  base::string16 out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::BigString16>(in, out));
  EXPECT_EQ(in, out);
}

TEST(BigString16Test, Long) {
  constexpr size_t kLargeStringSize = 1024 * 1024;

  base::string16 in(kLargeStringSize, 0);
  base::RandBytes(&in[0], kLargeStringSize);

  base::string16 out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::BigString16>(in, out));
  EXPECT_EQ(in, out);
}

}  // namespace string16_unittest
}  // namespace mojo_base
