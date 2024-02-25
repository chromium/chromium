// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ipcz_driver/ring_buffer.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/ranges/algorithm.h"
#include "mojo/core/ipcz_driver/shared_buffer.h"
#include "mojo/core/ipcz_driver/shared_buffer_mapping.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo::core::ipcz_driver {
namespace {

using RingBufferTest = testing::Test;

std::string_view AsString(base::span<const uint8_t> bytes) {
  return std::string_view(reinterpret_cast<const char*>(bytes.data()),
                          bytes.size());
}

base::span<const uint8_t> AsBytes(std::string_view s) {
  return base::as_bytes(base::make_span(s)).first(s.length());
}

// Wraps a RingBuffer with more convient string-based I/O for tests to use.
class TestRingBuffer {
 public:
  explicit TestRingBuffer(size_t n) : buffer_(MapMemory(n)) {}
  explicit TestRingBuffer(SharedBufferMapping& mapping)
      : buffer_(base::WrapRefCounted(&mapping)) {}

  RingBuffer& buffer() { return buffer_; }

  size_t Write(std::string_view s) { return buffer_.Write(AsBytes(s)); }

  bool WriteAll(std::string_view s) { return buffer_.WriteAll(AsBytes(s)); }

  std::string Read(size_t n) {
    std::vector<uint8_t> data(n);
    auto bytes = base::make_span(data);
    const size_t size = buffer_.Read(bytes);
    return std::string(AsString(bytes.first(size)));
  }

  std::optional<std::string> ReadAll(size_t n) {
    std::vector<uint8_t> data(n);
    if (!buffer_.ReadAll(base::make_span(data))) {
      return std::nullopt;
    }
    return std::string(data.begin(), data.end());
  }

  std::string Peek(size_t n) {
    std::vector<uint8_t> data(n);
    auto bytes = base::make_span(data);
    const size_t size = buffer_.Peek(bytes);
    return std::string(AsString(bytes.first(size)));
  }

  std::optional<std::string> PeekAll(size_t n) {
    std::vector<uint8_t> data(n);
    if (!buffer_.PeekAll(base::make_span(data))) {
      return std::nullopt;
    }
    return std::string(data.begin(), data.end());
  }

 private:
  static scoped_refptr<SharedBufferMapping> MapMemory(size_t size) {
    auto buffer = SharedBuffer::MakeForRegion(
        base::UnsafeSharedMemoryRegion::Create(size));
    return SharedBufferMapping::Create(buffer->region());
  }

  RingBuffer buffer_;
};

TEST_F(RingBufferTest, EmptyReads) {
  TestRingBuffer ring(256);
  EXPECT_EQ(0u, ring.buffer().data_size());
  EXPECT_EQ(256u, ring.buffer().available_capacity());

  uint8_t bytes[1];
  EXPECT_EQ(0u, ring.buffer().Read(bytes));
  EXPECT_EQ(0u, ring.buffer().Peek(bytes));
  EXPECT_FALSE(ring.buffer().ReadAll(bytes));
  EXPECT_FALSE(ring.buffer().PeekAll(bytes));

  RingBuffer::DirectReader reader(ring.buffer());
  EXPECT_TRUE(reader.bytes().empty());
  EXPECT_FALSE(std::move(reader).Consume(1));
}

TEST_F(RingBufferTest, FullWrites) {
  uint8_t data[256] = {0};
  TestRingBuffer ring(256);
  ASSERT_TRUE(ring.buffer().WriteAll(data));
  EXPECT_EQ(256u, ring.buffer().data_size());
  EXPECT_EQ(0u, ring.buffer().available_capacity());

  const uint8_t bytes[] = {42};
  EXPECT_EQ(0u, ring.buffer().Write(bytes));
  EXPECT_FALSE(ring.buffer().WriteAll(bytes));

  RingBuffer::DirectWriter writer(ring.buffer());
  EXPECT_TRUE(writer.bytes().empty());
  EXPECT_FALSE(std::move(writer).Commit(1));
}

TEST_F(RingBufferTest, DirectReader) {
  TestRingBuffer ring(8);
  ASSERT_TRUE(ring.WriteAll("abcdefgh"));

  // Since this is a fresh buffer, the data starts at the front of the buffer
  // and a DirectReader can therefore see all of it.
  {
    RingBuffer::DirectReader reader(ring.buffer());
    EXPECT_EQ(reader.bytes().size(), ring.buffer().data_size());
    EXPECT_EQ("abcdefgh", AsString(reader.bytes()));

    // Consume 3 bytes and replace them with "xyz".
    EXPECT_TRUE(std::move(reader).Consume(3));
    EXPECT_EQ(5u, ring.buffer().data_size());
    EXPECT_EQ(3u, ring.buffer().available_capacity());
    ASSERT_TRUE(ring.WriteAll("xyz"));
    EXPECT_EQ(8u, ring.buffer().data_size());
    EXPECT_EQ(0u, ring.buffer().available_capacity());
  }

  // The buffer should contain "xyzdefgh" now, with the next read starting at
  // the 'd'. The DirectReader cannot see the full contents, because they wrap
  // around to the front of the buffer.
  {
    RingBuffer::DirectReader reader(ring.buffer());
    EXPECT_EQ(5u, reader.bytes().size());
    EXPECT_EQ("defgh", AsString(reader.bytes()));

    // Consume 4 bytes and replace them with "1234".
    EXPECT_TRUE(std::move(reader).Consume(4));
    EXPECT_EQ(4u, ring.buffer().data_size());
    EXPECT_EQ(4u, ring.buffer().available_capacity());
    ASSERT_TRUE(ring.WriteAll("1234"));
    EXPECT_EQ(8u, ring.buffer().data_size());
    EXPECT_EQ(0u, ring.buffer().available_capacity());
  }

  // Still only the end of the physical buffer is visible.
  {
    RingBuffer::DirectReader reader(ring.buffer());
    EXPECT_EQ(1u, reader.bytes().size());
    EXPECT_EQ("h", AsString(reader.bytes()));
    EXPECT_TRUE(std::move(reader).Consume(1));
  }

  // Now that the end of the buffer has been consumed a new DirectReader can
  // see the remaining data, starting at the front of the buffer.
  {
    RingBuffer::DirectReader reader(ring.buffer());
    EXPECT_EQ(7u, reader.bytes().size());
    EXPECT_EQ("xyz1234", AsString(reader.bytes()));
    EXPECT_TRUE(std::move(reader).Consume(3));
  }

  ASSERT_TRUE(ring.WriteAll("!!!!"));

  // DirectReader can't consume more than it exposes, even if there is more data
  // available to read elsewhere.
  RingBuffer::DirectReader reader(ring.buffer());
  EXPECT_EQ(5u, reader.bytes().size());
  EXPECT_EQ("1234!", AsString(reader.bytes()));
  EXPECT_FALSE(std::move(reader).Consume(6));
  EXPECT_EQ("1234!!!!", *ring.ReadAll(8));
}

TEST_F(RingBufferTest, DirectWriter) {
  TestRingBuffer ring(8);

  // A DirectWriter can initially see all capacity in the buffer, because the
  // the buffer is empty and the next available byte is at the front.
  {
    RingBuffer::DirectWriter writer(ring.buffer());
    EXPECT_EQ(8u, writer.bytes().size());
    EXPECT_EQ(0u, ring.buffer().data_size());
    EXPECT_EQ(8u, ring.buffer().available_capacity());

    base::ranges::copy(AsBytes("abc"), writer.bytes().begin());
    EXPECT_TRUE(std::move(writer).Commit(3));
    EXPECT_EQ(3u, ring.buffer().data_size());
    EXPECT_EQ(5u, ring.buffer().available_capacity());
  }

  EXPECT_EQ("ab", ring.ReadAll(2));

  // Although there are now 7 bytes of available capacity, only 5 of them are
  // contiguous with the starting byte.
  {
    RingBuffer::DirectWriter writer(ring.buffer());
    EXPECT_EQ(1u, ring.buffer().data_size());
    EXPECT_EQ(7u, ring.buffer().available_capacity());
    EXPECT_EQ(5u, writer.bytes().size());

    base::ranges::copy(AsBytes("defgh"), writer.bytes().begin());
    EXPECT_TRUE(std::move(writer).Commit(5));
  }

  EXPECT_EQ("cdefgh", ring.PeekAll(6));
  EXPECT_EQ("cde", ring.ReadAll(3));

  // Now available capacity starts at the front of the buffer again.
  {
    RingBuffer::DirectWriter writer(ring.buffer());
    EXPECT_EQ(3u, ring.buffer().data_size());
    EXPECT_EQ(5u, ring.buffer().available_capacity());
    EXPECT_EQ(5u, writer.bytes().size());

    base::ranges::copy(AsBytes("12345"), writer.bytes().begin());
    EXPECT_TRUE(std::move(writer).Commit(5));
  }

  EXPECT_EQ("fgh12345", ring.PeekAll(8));
  EXPECT_FALSE(RingBuffer::DirectWriter(ring.buffer()).Commit(1));
}

TEST_F(RingBufferTest, BasicWrite) {
  TestRingBuffer ring(8);

  EXPECT_EQ(4u, ring.Write("hihi"));
  EXPECT_EQ(4u, ring.buffer().data_size());
  EXPECT_EQ(4u, ring.buffer().available_capacity());
  EXPECT_EQ("hihi", ring.Peek(8));

  EXPECT_EQ(1u, ring.buffer().Discard(1));
  EXPECT_EQ(5u, ring.Write("hehe! hehe!"));
  EXPECT_EQ(8u, ring.buffer().data_size());
  EXPECT_EQ(0u, ring.buffer().available_capacity());
  EXPECT_EQ("ihihehe!", ring.Peek(8));

  EXPECT_TRUE(ring.buffer().Discard(2));
  EXPECT_EQ(6u, ring.buffer().data_size());
  EXPECT_EQ(2u, ring.buffer().available_capacity());
  EXPECT_EQ("ihehe!", ring.Peek(6));

  EXPECT_EQ(2u, ring.Write("!!!!!!!!!"));
  EXPECT_EQ("ihehe!!!", ring.PeekAll(8));
}

TEST_F(RingBufferTest, BasicRead) {
  TestRingBuffer ring(8);
  EXPECT_TRUE(ring.WriteAll("abcdefgh"));
  EXPECT_EQ(8u, ring.buffer().data_size());
  EXPECT_EQ(0u, ring.buffer().available_capacity());

  EXPECT_EQ("abc", ring.ReadAll(3));
  EXPECT_EQ(5u, ring.buffer().data_size());
  EXPECT_EQ(3u, ring.buffer().available_capacity());

  EXPECT_EQ("d", ring.Read(1));
  EXPECT_EQ(4u, ring.buffer().data_size());
  EXPECT_EQ(4u, ring.buffer().available_capacity());

  EXPECT_TRUE(ring.WriteAll("1234"));
  EXPECT_EQ(8u, ring.buffer().data_size());
  EXPECT_EQ(0u, ring.buffer().available_capacity());

  EXPECT_EQ("efgh1234", ring.Peek(8));

  EXPECT_EQ("efgh12", ring.Read(6));
  EXPECT_EQ(2u, ring.buffer().data_size());
  EXPECT_EQ(6u, ring.buffer().available_capacity());

  EXPECT_TRUE(ring.WriteAll("xyzw"));
  EXPECT_EQ("34xyzw", ring.Read(6));
  EXPECT_EQ(0u, ring.buffer().data_size());
  EXPECT_EQ(8u, ring.buffer().available_capacity());
}

TEST_F(RingBufferTest, ExtendDataRange) {
  TestRingBuffer ring(8);
  base::ranges::copy(AsBytes("abcdefgh"),
                     ring.buffer().mapping().bytes().begin());
  EXPECT_EQ(0u, ring.buffer().data_size());
  EXPECT_EQ(8u, ring.buffer().available_capacity());

  // Too large.
  EXPECT_FALSE(ring.buffer().ExtendDataRange(9));
  EXPECT_EQ(0u, ring.buffer().data_size());
  EXPECT_EQ(8u, ring.buffer().available_capacity());

  // Include the first 5 bytes of the buffer as readable data.
  EXPECT_TRUE(ring.buffer().ExtendDataRange(5));
  EXPECT_EQ(5u, ring.buffer().data_size());
  EXPECT_EQ(3u, ring.buffer().available_capacity());

  // Now consume two bytes and add two more to the end.
  EXPECT_EQ("abcde", ring.Peek(8));
  EXPECT_EQ("ab", ring.ReadAll(2));
  EXPECT_EQ(3u, ring.buffer().data_size());
  EXPECT_EQ(5u, ring.buffer().available_capacity());
  EXPECT_TRUE(ring.buffer().ExtendDataRange(2));
  EXPECT_EQ(5u, ring.buffer().data_size());
  EXPECT_EQ(3u, ring.buffer().available_capacity());
  EXPECT_EQ("cdefg", ring.Peek(8));

  // Finally, extend further so the data wraps around to the front of the
  // buffer. All bytes should be included, with next available being 'c'.
  EXPECT_TRUE(ring.buffer().ExtendDataRange(3));
  EXPECT_EQ(8u, ring.buffer().data_size());
  EXPECT_EQ(0u, ring.buffer().available_capacity());
  EXPECT_EQ("cdefghab", ring.Peek(8));
}

TEST_F(RingBufferTest, BoundaryChecks) {
  TestRingBuffer ring(8);
  EXPECT_FALSE(ring.WriteAll("123456789"));
  EXPECT_FALSE(ring.ReadAll(1));
  EXPECT_FALSE(ring.PeekAll(1));
  EXPECT_FALSE(ring.buffer().DiscardAll(1));
  EXPECT_EQ("", ring.Read(1));
  EXPECT_EQ("", ring.Peek(1));
  EXPECT_EQ(0u, ring.buffer().Discard(1));

  EXPECT_TRUE(ring.WriteAll("12345678"));
  EXPECT_FALSE(ring.WriteAll("!"));
  EXPECT_EQ(0u, ring.Write("!"));
}

TEST_F(RingBufferTest, Serialization) {
  TestRingBuffer ring(8);
  EXPECT_TRUE(ring.WriteAll("hello!"));
  EXPECT_TRUE(ring.buffer().DiscardAll(1));
  EXPECT_EQ("ello!", ring.Peek(8));

  RingBuffer::SerializedState state;
  ring.buffer().Serialize(state);

  TestRingBuffer new_ring(ring.buffer().mapping());
  EXPECT_TRUE(new_ring.buffer().Deserialize(state));
  EXPECT_EQ("ello!", new_ring.Peek(8));

  TestRingBuffer bad_ring(ring.buffer().mapping());
  // Invalid offset.
  EXPECT_FALSE(bad_ring.buffer().Deserialize({.offset = 8, .size = 1}));
  // Invalid size.
  EXPECT_FALSE(bad_ring.buffer().Deserialize({.offset = 0, .size = 9}));
}

}  // namespace
}  // namespace mojo::core::ipcz_driver
