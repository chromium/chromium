// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_buffer.h"
#include "media/base/decoder_buffer_queue.h"
#include "media/base/timestamp_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static base::TimeDelta ToTimeDelta(int seconds) {
  if (seconds < 0)
    return kNoTimestamp;
  return base::Seconds(seconds);
}

// Helper to create buffers with specified timestamp in seconds.
//
// Negative numbers will be converted to kNoTimestamp;
static scoped_refptr<DecoderBuffer> CreateBuffer(int timestamp) {
  scoped_refptr<DecoderBuffer> buffer = new DecoderBuffer(0);
  buffer->set_timestamp(ToTimeDelta(timestamp));
  buffer->set_duration(ToTimeDelta(0));
  return buffer;
}

static scoped_refptr<DecoderBuffer> CreateBuffer(int timestamp, int size) {
  scoped_refptr<DecoderBuffer> buffer = new DecoderBuffer(size);
  buffer->set_timestamp(ToTimeDelta(timestamp));
  buffer->set_duration(ToTimeDelta(0));
  return buffer;
}

static size_t GetExpectedMemoryUsage(int number_of_buffers, int data_size) {
  return number_of_buffers * sizeof(DecoderBuffer) + data_size;
}

TEST(DecoderBufferQueueTest, IsEmpty) {
  DecoderBufferQueue queue;
  EXPECT_TRUE(queue.IsEmpty());

  queue.Push(CreateBuffer(0));
  EXPECT_FALSE(queue.IsEmpty());
}

TEST(DecoderBufferQueueTest, Clear) {
  DecoderBufferQueue queue;
  queue.Push(CreateBuffer(0));
  queue.Push(CreateBuffer(1));
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(1, queue.Duration().InSeconds());

  queue.Clear();
  EXPECT_TRUE(queue.IsEmpty());
  EXPECT_EQ(0, queue.Duration().InSeconds());
}

TEST(DecoderBufferQueueTest, Duration) {
  DecoderBufferQueue queue;
  EXPECT_EQ(0, queue.Duration().InSeconds());

  queue.Push(CreateBuffer(0));
  EXPECT_EQ(0, queue.Duration().InSeconds());

  queue.Push(CreateBuffer(1));
  EXPECT_EQ(1, queue.Duration().InSeconds());

  queue.Push(CreateBuffer(2));
  EXPECT_EQ(2, queue.Duration().InSeconds());

  queue.Push(CreateBuffer(4));
  EXPECT_EQ(4, queue.Duration().InSeconds());

  queue.Pop();
  EXPECT_EQ(3, queue.Duration().InSeconds());

  queue.Pop();
  EXPECT_EQ(2, queue.Duration().InSeconds());

  queue.Pop();
  EXPECT_EQ(0, queue.Duration().InSeconds());

  queue.Pop();
  EXPECT_EQ(0, queue.Duration().InSeconds());
}

TEST(DecoderBufferQueueTest, Duration_OutOfOrder) {
  DecoderBufferQueue queue;
  queue.Push(CreateBuffer(10));
  queue.Push(CreateBuffer(12));
  EXPECT_EQ(2, queue.Duration().InSeconds());

  // Out of order: duration shouldn't change.
  queue.Push(CreateBuffer(8));
  EXPECT_EQ(2, queue.Duration().InSeconds());

  // Removing first buffer should leave the second buffer as the only buffer
  // included in the duration calculation.
  queue.Pop();
  EXPECT_EQ(0, queue.Duration().InSeconds());

  // Removing second buffer leaves the out-of-order buffer. It shouldn't be
  // included in duration calculations.
  queue.Pop();
  EXPECT_EQ(0, queue.Duration().InSeconds());

  // Push a still-too-early buffer. It shouldn't be included in duration
  // calculations.
  queue.Push(CreateBuffer(11));
  EXPECT_EQ(0, queue.Duration().InSeconds());

  // Push a buffer that's after the earliest valid time. It's a singular valid
  // buffer so duration is still zero.
  queue.Push(CreateBuffer(14));
  EXPECT_EQ(0, queue.Duration().InSeconds());

  // Push a second valid buffer. We should now have a duration.
  queue.Push(CreateBuffer(17));
  EXPECT_EQ(3, queue.Duration().InSeconds());
}

TEST(DecoderBufferQueueTest, Duration_NoTimestamp) {
  // Buffers with no timestamp don't affect duration.
  DecoderBufferQueue queue;
  queue.Push(CreateBuffer(0));
  queue.Push(CreateBuffer(4));
  EXPECT_EQ(4, queue.Duration().InSeconds());

  queue.Push(CreateBuffer(-1));
  EXPECT_EQ(4, queue.Duration().InSeconds());

  queue.Push(CreateBuffer(6));
  EXPECT_EQ(6, queue.Duration().InSeconds());

  queue.Pop();
  EXPECT_EQ(2, queue.Duration().InSeconds());

  queue.Pop();
  EXPECT_EQ(0, queue.Duration().InSeconds());

  queue.Pop();
  EXPECT_EQ(0, queue.Duration().InSeconds());

  queue.Pop();
  EXPECT_EQ(0, queue.Duration().InSeconds());
}

TEST(DecoderBufferQueueTest, MemoryUsage) {
  DecoderBufferQueue queue;
  EXPECT_EQ(queue.memory_usage_in_bytes(), 0u);

  queue.Push(CreateBuffer(0, 0u));
  EXPECT_EQ(queue.memory_usage_in_bytes(), GetExpectedMemoryUsage(1, 0));

  queue.Pop();
  EXPECT_EQ(queue.memory_usage_in_bytes(), 0u);

  queue.Push(CreateBuffer(1, 1200u));
  EXPECT_EQ(queue.memory_usage_in_bytes(), GetExpectedMemoryUsage(1, 1200));

  queue.Push(CreateBuffer(2, 1000u));
  EXPECT_EQ(queue.memory_usage_in_bytes(), GetExpectedMemoryUsage(2, 2200));

  queue.Pop();
  EXPECT_EQ(queue.memory_usage_in_bytes(), GetExpectedMemoryUsage(1, 1000));

  queue.Push(CreateBuffer(3, 999u));
  queue.Push(CreateBuffer(4, 999u));
  EXPECT_EQ(queue.memory_usage_in_bytes(), GetExpectedMemoryUsage(3, 2998));

  queue.Clear();
  EXPECT_EQ(queue.memory_usage_in_bytes(), GetExpectedMemoryUsage(0, 0));

  queue.Push(CreateBuffer(5, 1400u));
  EXPECT_EQ(queue.memory_usage_in_bytes(), GetExpectedMemoryUsage(1, 1400));
}

}  // namespace media
