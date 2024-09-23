// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/rand_util.h"
#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/string16.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/mojo/string16_mojom_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

TEST(String16MojomTraitsTest, String16) {
  // |str| is 8-bit.
  String str = String::FromUTF8("hello world");
  String output;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojo_base::mojom::blink::String16>(
          str, output));
  ASSERT_EQ(str, output);

  // Replace the "o"s in "hello world" with "o"s with acute, so that |str| is
  // 16-bit.
  str = String::FromUTF8("hell\xC3\xB3 w\xC3\xB3rld");

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojo_base::mojom::blink::String16>(
          str, output));
  ASSERT_EQ(str, output);
}

TEST(String16MojomTraitsTest, EmptyString16) {
  String str = String::FromUTF8("");
  String output;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojo_base::mojom::blink::String16>(
          str, output));
  ASSERT_EQ(str, output);
}

TEST(String16MojomTraitsTest, BigString16_Empty) {
  String str = String::FromUTF8("");
  String output;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojo_base::mojom::blink::BigString16>(
          str, output));
  ASSERT_EQ(str, output);
}

TEST(String16MojomTraitsTest, BigString16_Short) {
  String str = String::FromUTF8("hello world");
  ASSERT_TRUE(str.Is8Bit());
  String output;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojo_base::mojom::blink::BigString16>(
          str, output));
  ASSERT_EQ(str, output);

  // Replace the "o"s in "hello world" with "o"s with acute, so that |str| is
  // 16-bit.
  str = String::FromUTF8("hell\xC3\xB3 w\xC3\xB3rld");

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojo_base::mojom::blink::BigString16>(
          str, output));
  ASSERT_EQ(str, output);
}

TEST(String16MojomTraitsTest, BigString16_Long) {
  WTF::Vector<char> random_latin1_string(1024 * 1024);
  base::RandBytes(base::as_writable_byte_span(random_latin1_string));

  String str(random_latin1_string.data(), random_latin1_string.size());
  String output;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojo_base::mojom::blink::BigString16>(
          str, output));
  ASSERT_EQ(str, output);
}

}  // namespace blink
