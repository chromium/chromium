// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): IOBuffer provides access to its data via a raw
// pointer. Either remove the accessors that provide access to the raw pointer,
// and remove this annotation and the tests for them, or add `UNSAFE_BUFFERS`
// annotations around the code that tests those accessors.
#pragma allow_unsafe_buffers
#endif

#include "net/base/io_buffer.h"

#include <array>

#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/checked_math.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Compares the data in `io_buffer` to `span`. Tests all const accessors.
void CompareConstIOBufferToSpan(const IOBuffer& io_buffer,
                                base::span<const uint8_t> span) {
  ASSERT_EQ(io_buffer.span(), span);
  ASSERT_EQ(base::checked_cast<size_t>(io_buffer.size()), span.size());
  EXPECT_EQ(base::span(reinterpret_cast<const uint8_t*>(io_buffer.bytes()),
                       static_cast<size_t>(io_buffer.size())),
            span);
  EXPECT_EQ(
      base::span(io_buffer.bytes(), static_cast<size_t>(io_buffer.size())),
      span);
  EXPECT_EQ(io_buffer.span(), span);
}

// Compares the data in `io_buffer` to `span`. Tests all accessors.
void CompareIOBufferToSpan(IOBuffer& io_buffer,
                           base::span<const uint8_t> span) {
  CompareConstIOBufferToSpan(io_buffer, span);

  ASSERT_EQ(io_buffer.span(), span);
  ASSERT_EQ(base::checked_cast<size_t>(io_buffer.size()), span.size());
  EXPECT_EQ(base::span(reinterpret_cast<const uint8_t*>(io_buffer.bytes()),
                       static_cast<size_t>(io_buffer.size())),
            span);
  EXPECT_EQ(
      base::span(io_buffer.bytes(), static_cast<size_t>(io_buffer.size())),
      span);
  EXPECT_EQ(io_buffer.span(), span);
}

TEST(IOBufferTest, VectorIOBuffer) {
  // Include values greater than 0x7F to make sure signed/unsigned is handled
  // appropriately.
  std::vector<uint8_t> data{0, 0xFF, 1, 0xFE, 2, 0xFD, 3, 0xFC,
                            4, 0xFB, 5, 0xFA, 6, 0xF9, 7, 0x80};
  std::vector<uint8_t> original_data = data;
  auto buffer = base::MakeRefCounted<VectorIOBuffer>(data);
  CompareIOBufferToSpan(*buffer, data);

  // Check that the buffer has its own copy of the data.
  EXPECT_NE(buffer->bytes(), data.data());

  // Test writing to the buffer.
  buffer->span()[0] = 9;
  buffer->span()[data.size() - 1] = 0x90;
  std::vector<uint8_t> data2{9, 0xFF, 1, 0xFE, 2, 0xFD, 3, 0xFC,
                             4, 0xFB, 5, 0xFA, 6, 0xF9, 7, 0x90};
  CompareIOBufferToSpan(*buffer, data2);
  // The original buffer should not have been modified.
  EXPECT_EQ(original_data, data);
  EXPECT_NE(buffer->span(), data);
}

TEST(IOBufferTest, StringIOBuffer) {
  // Include values greater than 0x7F to make sure signed/unsigned is handled
  // appropriately.
  auto data = std::array{'\0', 'a', 'b', 'c', '\xFF', '\xF0'};
  auto buffer = base::MakeRefCounted<StringIOBuffer>(
      std::string(data.begin(), data.end()));
  CompareIOBufferToSpan(*buffer, base::as_bytes(base::span(data)));

  // Check that the buffer has its own copy of the data.
  EXPECT_NE(buffer->data(), data.data());
}

TEST(IOBufferTest, GrowableIOBuffer_SpanBeforeOffset) {
  auto buffer = base::MakeRefCounted<GrowableIOBuffer>();
  buffer->SetCapacity(100);
  EXPECT_EQ(0u, buffer->span_before_offset().size());

  buffer->set_offset(10);
  EXPECT_EQ(10u, buffer->span_before_offset().size());
  EXPECT_EQ(buffer->everything().data(), buffer->span_before_offset().data());

  buffer->set_offset(100);
  EXPECT_EQ(100u, buffer->span_before_offset().size());
  EXPECT_EQ(buffer->everything().data(), buffer->span_before_offset().data());
}

TEST(IOBufferTest, WrappedIOBuffer) {
  // Include values greater than 0x7F to make sure signed/unsigned is handled
  // appropriately.
  std::vector<uint8_t> data{0, 0xFF, 1, 0xFE, 2, 0xFD, 3, 0xFC,
                            4, 0xFB, 5, 0xFA, 6, 0xF9, 7, 0x80};
  auto buffer = base::MakeRefCounted<WrappedIOBuffer>(data);
  CompareIOBufferToSpan(*buffer, data);

  // Check that the buffer does not have its own copy of the data.
  EXPECT_EQ(buffer->bytes(), data.data());
}

}  // anonymous namespace

}  // namespace net
