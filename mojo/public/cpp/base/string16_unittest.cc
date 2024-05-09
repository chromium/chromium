// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

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
  std::u16string in;
  std::u16string out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::String16>(in, out));
  EXPECT_EQ(in, out);
}

TEST(String16Test, NonEmpty) {
  std::u16string in = u"hello world";
  std::u16string out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::String16>(in, out));
  EXPECT_EQ(in, out);
}

TEST(BigString16Test, Empty) {
  std::u16string in;
  std::u16string out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::BigString16>(in, out));
  EXPECT_EQ(in, out);
}

TEST(BigString16Test, Short) {
  std::u16string in = u"hello world";
  std::u16string out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::BigString16>(in, out));
  EXPECT_EQ(in, out);
}

TEST(BigString16Test, Long) {
  constexpr size_t kLargeStringSize = 1024 * 1024;

  std::u16string in(kLargeStringSize, 0);
  base::RandBytes(base::as_writable_byte_span(in));

  std::u16string out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::BigString16>(in, out));
  EXPECT_EQ(in, out);
}

}  // namespace string16_unittest
}  // namespace mojo_base
