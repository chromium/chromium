// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <algorithm>
#include <vector>

#include "base/rand_util.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/big_buffer.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {
namespace big_buffer_unittest {

namespace {

bool BufferEquals(const BigBuffer& a, const BigBuffer& b) {
  return a.size() == b.size() && std::equal(a.data(), a.data() + a.size(),
                                            b.data(), b.data() + b.size());
}

}  // namespace

TEST(BigBufferTest, EmptyBuffer) {
  BigBuffer in;
  BigBuffer out;
  EXPECT_EQ(BigBuffer::StorageType::kBytes, in.storage_type());
  EXPECT_EQ(0u, in.size());

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::BigBuffer>(in, out));

  EXPECT_EQ(BigBuffer::StorageType::kBytes, out.storage_type());
  EXPECT_TRUE(BufferEquals(in, out));
}

TEST(BigBufferTest, SmallDataSize) {
  BigBuffer in(std::array<uint8_t, 3>{1, 2, 3});
  EXPECT_EQ(BigBuffer::StorageType::kBytes, in.storage_type());

  BigBuffer out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::BigBuffer>(in, out));

  EXPECT_EQ(BigBuffer::StorageType::kBytes, out.storage_type());
  EXPECT_TRUE(BufferEquals(in, out));
}

TEST(BigBufferTest, LargeDataSize) {
  constexpr size_t kLargeDataSize = BigBuffer::kMaxInlineBytes * 2;
  std::array<uint8_t, kLargeDataSize> data;
  base::RandBytes(data);

  BigBuffer in(data);
  EXPECT_EQ(BigBuffer::StorageType::kSharedMemory, in.storage_type());

  BigBuffer out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::BigBuffer>(in, out));

  EXPECT_EQ(BigBuffer::StorageType::kSharedMemory, out.storage_type());

  // NOTE: It's not safe to compare to |in| here since serialization will have
  // taken ownership of its internal shared buffer handle.
  EXPECT_TRUE(BufferEquals(base::span<const uint8_t>(data), out));
}

TEST(BigBufferTest, InvalidBuffer) {
  // Verifies that deserializing invalid BigBuffers and BigBufferViews always
  // fails.

  BigBufferView out_view;
  auto invalid_view = BigBufferView::CreateInvalidForTest();
  EXPECT_EQ(invalid_view.storage_type(),
            BigBuffer::StorageType::kInvalidBuffer);
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::BigBuffer>(
      invalid_view, out_view));

  BigBuffer out_buffer;
  auto invalid_buffer =
      BigBufferView::ToBigBuffer(BigBufferView::CreateInvalidForTest());
  EXPECT_EQ(invalid_buffer.storage_type(),
            BigBuffer::StorageType::kInvalidBuffer);
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::BigBuffer>(
      invalid_buffer, out_buffer));
}

}  // namespace big_buffer_unittest
}  // namespace mojo_base
