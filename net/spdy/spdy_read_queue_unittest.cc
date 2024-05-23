// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/spdy/spdy_read_queue.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "net/spdy/spdy_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::test {
namespace {

const char kData[] = "SPDY read queue test data.\0Some more data.";
const size_t kDataSize = std::size(kData);

// Enqueues |data| onto |queue| in chunks of at most |max_buffer_size|
// bytes.
void EnqueueString(const std::string& data,
                   size_t max_buffer_size,
                   SpdyReadQueue* queue) {
  ASSERT_GT(data.size(), 0u);
  ASSERT_GT(max_buffer_size, 0u);
  size_t old_total_size = queue->GetTotalSize();
  for (size_t i = 0; i < data.size();) {
    size_t buffer_size = std::min(data.size() - i, max_buffer_size);
    queue->Enqueue(std::make_unique<SpdyBuffer>(data.data() + i, buffer_size));
    i += buffer_size;
    EXPECT_FALSE(queue->IsEmpty());
    EXPECT_EQ(old_total_size + i, queue->GetTotalSize());
  }
}

// Dequeues all bytes in |queue| in chunks of at most
// |max_buffer_size| bytes and returns the data as a string.
std::string DrainToString(size_t max_buffer_size, SpdyReadQueue* queue) {
  std::string data;

  // Pad the buffer so we can detect out-of-bound writes.
  size_t padding = std::max(static_cast<size_t>(4096), queue->GetTotalSize());
  size_t buffer_size_with_padding = padding + max_buffer_size + padding;
  auto buffer = std::make_unique<char[]>(buffer_size_with_padding);
  std::memset(buffer.get(), 0, buffer_size_with_padding);
  char* buffer_data = buffer.get() + padding;

  while (!queue->IsEmpty()) {
    size_t old_total_size = queue->GetTotalSize();
    EXPECT_GT(old_total_size, 0u);
    size_t dequeued_bytes = queue->Dequeue(buffer_data, max_buffer_size);

    // Make sure |queue| doesn't write past either end of its given
    // boundaries.
    for (int i = 1; i <= static_cast<int>(padding); ++i) {
      EXPECT_EQ('\0', buffer_data[-i]) << -i;
    }
    for (size_t i = 0; i < padding; ++i) {
      EXPECT_EQ('\0', buffer_data[max_buffer_size + i]) << i;
    }

    data.append(buffer_data, dequeued_bytes);
    EXPECT_EQ(dequeued_bytes, std::min(max_buffer_size, dequeued_bytes));
    EXPECT_EQ(queue->GetTotalSize(), old_total_size - dequeued_bytes);
  }
  EXPECT_TRUE(queue->IsEmpty());
  return data;
}

// Enqueue a test string with the given enqueue/dequeue max buffer
// sizes.
void RunEnqueueDequeueTest(size_t enqueue_max_buffer_size,
                           size_t dequeue_max_buffer_size) {
  std::string data(kData, kDataSize);
  SpdyReadQueue read_queue;
  EnqueueString(data, enqueue_max_buffer_size, &read_queue);
  const std::string& drained_data =
      DrainToString(dequeue_max_buffer_size, &read_queue);
  EXPECT_EQ(data, drained_data);
}

void OnBufferDiscarded(bool* discarded,
                       size_t* discarded_bytes,
                       size_t delta,
                       SpdyBuffer::ConsumeSource consume_source) {
  EXPECT_EQ(SpdyBuffer::DISCARD, consume_source);
  *discarded = true;
  *discarded_bytes = delta;
}

}  // namespace

class SpdyReadQueueTest : public ::testing::Test {};

// Call RunEnqueueDequeueTest() with various buffer size combinatinos.

TEST_F(SpdyReadQueueTest, LargeEnqueueAndDequeueBuffers) {
  RunEnqueueDequeueTest(2 * kDataSize, 2 * kDataSize);
}

TEST_F(SpdyReadQueueTest, OneByteEnqueueAndDequeueBuffers) {
  RunEnqueueDequeueTest(1, 1);
}

TEST_F(SpdyReadQueueTest, CoprimeBufferSizes) {
  RunEnqueueDequeueTest(2, 3);
  RunEnqueueDequeueTest(3, 2);
}

TEST_F(SpdyReadQueueTest, Clear) {
  auto buffer = std::make_unique<SpdyBuffer>(kData, kDataSize);
  bool discarded = false;
  size_t discarded_bytes = 0;
  buffer->AddConsumeCallback(
      base::BindRepeating(&OnBufferDiscarded, &discarded, &discarded_bytes));

  SpdyReadQueue read_queue;
  read_queue.Enqueue(std::move(buffer));

  EXPECT_FALSE(discarded);
  EXPECT_EQ(0u, discarded_bytes);
  EXPECT_FALSE(read_queue.IsEmpty());

  read_queue.Clear();

  EXPECT_TRUE(discarded);
  EXPECT_EQ(kDataSize, discarded_bytes);
  EXPECT_TRUE(read_queue.IsEmpty());
}

}  // namespace net::test
