// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ranges/algorithm.h"
#include "mojo/public/cpp/base/read_only_buffer_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/read_only_buffer.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {
namespace read_only_buffer_unittest {

TEST(ReadOnlyBufferTest, ReadOnlyBufferEmptySpan) {
  base::span<const uint8_t> in;
  base::span<const uint8_t> out;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ReadOnlyBuffer>(in, out));
  EXPECT_TRUE(base::ranges::equal(in, out));
}

TEST(ReadOnlyBufferTest, ReadOnlyBufferNonEmptySpan) {
  std::vector<uint8_t> v{'1', '2', '3'};
  base::span<const uint8_t> in(v);
  base::span<const uint8_t> out;

  // SerializeAndDeserialize doesn't work here: the traits for ReadOnlyBuffer
  // returns a span that points into the raw bytes in the mojo::Message;however,
  // the stack object will be freed before SerializeAndSerialize returns.
  std::vector<uint8_t> data = mojom::ReadOnlyBuffer::Serialize(&in);

  EXPECT_TRUE(mojom::ReadOnlyBuffer::Deserialize(std::move(data), &out));
  EXPECT_TRUE(base::ranges::equal(in, out));
}

}  // namespace read_only_buffer_unittest
}  // namespace mojo_base
