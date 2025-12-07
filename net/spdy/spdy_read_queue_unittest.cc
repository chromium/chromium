// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_read_queue.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_view_util.h"
#include "net/spdy/spdy_buffer.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Each;

namespace net::test {
namespace {

const auto kData = base::byte_span_with_nul_from_cstring(
    "SPDY read queue test data.\0Some more data.");

// Enqueues |data| onto |queue| in chunks of at most |max_buffer_size|
// bytes.
void EnqueueString(const std::string& data,
                   size_t max_buffer_size,
                   SpdyReadQueue* queue) {
  ASSERT_GT(data.size(), 0u);
  ASSERT_GT(max_buffer_size, 0u);
  size_t old_total_size = queue->GetTotalSize();

  auto data_span = base::as_byte_span(data);
  for (size_t i = 0; i < data.size();) {
    size_t buffer_size = std::min(data.size() - i, max_buffer_size);
    queue->Enqueue(
        std::make_unique<SpdyBuffer>(data_span.subspan(i, buffer_size)));
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
  auto buffer = base::HeapArray<uint8_t>::WithSize(buffer_size_with_padding);
  auto buffer_data = buffer.subspan(padding, max_buffer_size);

  while (!queue->IsEmpty()) {
    size_t old_total_size = queue->GetTotalSize();
    EXPECT_GT(old_total_size, 0u);
    size_t dequeued_bytes = queue->Dequeue(buffer_data);

    // Make sure |queue| doesn't write past either end of its given
    // boundaries.
    EXPECT_THAT(buffer.first(padding), Each(uint8_t{0}));
    EXPECT_THAT(buffer.last(padding), Each(uint8_t{0}));

    auto chunk = buffer_data.first(dequeued_bytes);
    data.append(base::as_string_view(chunk));
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
  std::string data(base::as_string_view(kData));
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
  RunEnqueueDequeueTest(2 * kData.size(), 2 * kData.size());
}

TEST_F(SpdyReadQueueTest, OneByteEnqueueAndDequeueBuffers) {
  RunEnqueueDequeueTest(1, 1);
}

TEST_F(SpdyReadQueueTest, CoprimeBufferSizes) {
  RunEnqueueDequeueTest(2, 3);
  RunEnqueueDequeueTest(3, 2);
}

TEST_F(SpdyReadQueueTest, Clear) {
  auto buffer = std::make_unique<SpdyBuffer>(kData);
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
  EXPECT_EQ(kData.size(), discarded_bytes);
  EXPECT_TRUE(read_queue.IsEmpty());
}

}  // namespace net::test
