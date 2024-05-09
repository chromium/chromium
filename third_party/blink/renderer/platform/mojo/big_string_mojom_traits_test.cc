// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"
#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/big_string.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/mojo/big_string_mojom_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(BigStringMojomTraitsTest, BigString_Null) {
  String str;
  String output;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojo_base::mojom::blink::BigString>(
          str, output));
  ASSERT_EQ(str, output);
}

TEST(BigStringMojomTraitsTest, BigString_Empty) {
  String str = String::FromUTF8("");
  String output;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojo_base::mojom::blink::BigString>(
          str, output));
  ASSERT_EQ(str, output);
}

TEST(BigStringMojomTraitsTest, BigString_Short) {
  String str = String::FromUTF8("hello world");
  ASSERT_TRUE(str.Is8Bit());
  String output;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojo_base::mojom::blink::BigString>(
          str, output));
  ASSERT_EQ(str, output);

  // Replace the "o"s in "hello world" with "o"s with acute, so that |str| is
  // 16-bit.
  str = String::FromUTF8("hell\xC3\xB3 w\xC3\xB3rld");
  ASSERT_FALSE(str.Is8Bit());

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojo_base::mojom::blink::BigString>(
          str, output));
  ASSERT_EQ(str, output);
}

TEST(BigStringMojomTraitsTest, BigString_Long) {
  WTF::Vector<char> random_latin1_string(1024 * 1024);
  base::RandBytes(base::as_writable_byte_span(random_latin1_string));

  String str(random_latin1_string.data(), random_latin1_string.size());
  String output;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojo_base::mojom::blink::BigString>(
          str, output));
  ASSERT_EQ(str, output);
}

}  // namespace blink
