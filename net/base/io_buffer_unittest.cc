// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  EXPECT_EQ(io_buffer.span(), span);

  // For validating the other accessors, it should be sufficient to check size()
  // and make sure returned pointers match io_buffer.span().data().
  ASSERT_EQ(base::checked_cast<size_t>(io_buffer.size()), span.size());
  EXPECT_EQ(reinterpret_cast<const uint8_t*>(io_buffer.data()),
            io_buffer.span().data());
  EXPECT_EQ(io_buffer.bytes(), io_buffer.span().data());
}

// Compares the data in `io_buffer` to `span`. Tests all accessors.
void CompareIOBufferToSpan(IOBuffer& io_buffer,
                           base::span<const uint8_t> span) {
  CompareConstIOBufferToSpan(io_buffer, span);

  // For validating the other accessors, it should be sufficient to check size()
  // and make sure returned pointers match io_buffer.span().data().
  ASSERT_EQ(base::checked_cast<size_t>(io_buffer.size()), span.size());
  EXPECT_EQ(reinterpret_cast<uint8_t*>(io_buffer.data()),
            io_buffer.span().data());
  EXPECT_EQ(io_buffer.bytes(), io_buffer.span().data());
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

TEST(IOBufferTest, DrainableIOBuffer_DidConsume) {
  // Include values greater than 0x7F to make sure signed/unsigned is handled
  // appropriately.
  std::vector<uint8_t> data{0, 0xFF, 1, 0xFE, 2, 0xFD, 3, 0xFC,
                            4, 0xFB, 5, 0xFA, 6, 0xF9, 7, 0x80};
  auto data_buffer = base::MakeRefCounted<VectorIOBuffer>(data);

  // Test both the case the entire nested IOBuffer is included in the
  // DrainableIOBuffer, and the case where the last value is excluded.
  for (size_t size : {data.size(), data.size() - 1}) {
    auto buffer = base::MakeRefCounted<DrainableIOBuffer>(data_buffer, size);
    auto span = base::span<uint8_t>(data).subspan(0u, size);
    CompareIOBufferToSpan(*buffer, span);
    EXPECT_EQ(buffer->BytesConsumed(), 0);
    EXPECT_EQ(buffer->BytesRemaining(), base::checked_cast<int>(size));

    buffer->DidConsume(1);
    CompareIOBufferToSpan(*buffer, span.subspan(1u));
    EXPECT_EQ(buffer->BytesConsumed(), 1);
    EXPECT_EQ(buffer->BytesRemaining(), base::checked_cast<int>(size - 1));

    buffer->DidConsume(3);
    CompareIOBufferToSpan(*buffer, span.subspan(4u));
    EXPECT_EQ(buffer->BytesConsumed(), 4);
    EXPECT_EQ(buffer->BytesRemaining(), base::checked_cast<int>(size - 4));

    buffer->DidConsume(buffer->size() - 1);
    CompareIOBufferToSpan(*buffer, span.subspan(size - 1));
    EXPECT_EQ(buffer->BytesConsumed(), base::checked_cast<int>(size - 1));
    EXPECT_EQ(buffer->BytesRemaining(), 1);

    buffer->DidConsume(1);
    CompareIOBufferToSpan(*buffer, base::span<uint8_t>());
    EXPECT_EQ(buffer->BytesConsumed(), base::checked_cast<int>(size));
    EXPECT_EQ(buffer->BytesRemaining(), 0);
  }
}

TEST(IOBufferTest, DrainableIOBuffer_SetOffset) {
  // Include values greater than 0x7F to make sure signed/unsigned is handled
  // appropriately.
  std::vector<uint8_t> data{0, 0xFF, 1, 0xFE, 2, 0xFD, 3, 0xFC,
                            4, 0xFB, 5, 0xFA, 6, 0xF9, 7, 0x80};
  auto data_buffer = base::MakeRefCounted<VectorIOBuffer>(data);

  // Test both the case the entire nested IOBuffer is included in the
  // DrainableIOBuffer, and the case where the last value is excluded.
  for (size_t size : {data.size(), data.size() - 1}) {
    auto buffer = base::MakeRefCounted<DrainableIOBuffer>(data_buffer, size);
    auto span = base::span<uint8_t>(data).subspan(0u, size);
    CompareIOBufferToSpan(*buffer, span);
    EXPECT_EQ(buffer->BytesConsumed(), 0);
    EXPECT_EQ(buffer->BytesRemaining(), base::checked_cast<int>(size));

    buffer->SetOffset(size - 1);
    CompareIOBufferToSpan(*buffer, span.subspan(size - 1));
    EXPECT_EQ(buffer->BytesConsumed(), base::checked_cast<int>(size - 1));
    EXPECT_EQ(buffer->BytesRemaining(), 1);

    buffer->SetOffset(1);
    CompareIOBufferToSpan(*buffer, span.subspan(1u));
    EXPECT_EQ(buffer->BytesConsumed(), 1);
    EXPECT_EQ(buffer->BytesRemaining(), base::checked_cast<int>(size - 1));

    buffer->SetOffset(size);
    CompareIOBufferToSpan(*buffer, base::span<uint8_t>());
    EXPECT_EQ(buffer->BytesConsumed(), base::checked_cast<int>(size));
    EXPECT_EQ(buffer->BytesRemaining(), 0);

    buffer->SetOffset(0);
    CompareIOBufferToSpan(*buffer, span);
    EXPECT_EQ(buffer->BytesConsumed(), 0);
    EXPECT_EQ(buffer->BytesRemaining(), base::checked_cast<int>(size));

    buffer->SetOffset(4);
    CompareIOBufferToSpan(*buffer, span.subspan(4u));
    EXPECT_EQ(buffer->BytesConsumed(), 4);
    EXPECT_EQ(buffer->BytesRemaining(), base::checked_cast<int>(size - 4));
  }
}

TEST(IOBufferTest, DrainableIOBuffer_DidConsumerAndSetOffset) {
  // Include values greater than 0x7F to make sure signed/unsigned is handled
  // appropriately.
  std::vector<uint8_t> data{0, 0xFF, 1, 0xFE, 2, 0xFD, 3, 0xFC,
                            4, 0xFB, 5, 0xFA, 6, 0xF9, 7, 0x80};
  auto data_buffer = base::MakeRefCounted<VectorIOBuffer>(data);

  // Test both the case the entire nested IOBuffer is included in the
  // DrainableIOBuffer, and the case where the last value is excluded.
  for (size_t size : {data.size(), data.size() - 1}) {
    auto buffer = base::MakeRefCounted<DrainableIOBuffer>(data_buffer, size);
    auto span = base::span<uint8_t>(data).subspan(0u, size);
    CompareIOBufferToSpan(*buffer, span);
    EXPECT_EQ(buffer->BytesConsumed(), 0);
    EXPECT_EQ(buffer->BytesRemaining(), base::checked_cast<int>(size));

    buffer->DidConsume(size);
    CompareIOBufferToSpan(*buffer, base::span<uint8_t>());
    EXPECT_EQ(buffer->BytesConsumed(), base::checked_cast<int>(size));
    EXPECT_EQ(buffer->BytesRemaining(), 0);

    buffer->SetOffset(1);
    CompareIOBufferToSpan(*buffer, span.subspan(1u));
    EXPECT_EQ(buffer->BytesConsumed(), 1);
    EXPECT_EQ(buffer->BytesRemaining(), base::checked_cast<int>(size - 1));

    buffer->DidConsume(3);
    CompareIOBufferToSpan(*buffer, span.subspan(4u));
    EXPECT_EQ(buffer->BytesConsumed(), 4);
    EXPECT_EQ(buffer->BytesRemaining(), base::checked_cast<int>(size - 4));

    buffer->SetOffset(0);
    CompareIOBufferToSpan(*buffer, span);
    EXPECT_EQ(buffer->BytesConsumed(), 0);
    EXPECT_EQ(buffer->BytesRemaining(), base::checked_cast<int>(size));

    buffer->DidConsume(4);
    CompareIOBufferToSpan(*buffer, span.subspan(4u));
    EXPECT_EQ(buffer->BytesConsumed(), 4);
    EXPECT_EQ(buffer->BytesRemaining(), base::checked_cast<int>(size - 4));
  }
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
