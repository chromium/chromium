// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "net/third_party/quic/core/quic_write_blocked_list.h"

#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"

using spdy::kV3HighestPriority;
using spdy::kV3LowestPriority;

namespace quic {
namespace test {
namespace {

class QuicWriteBlockedListTest : public QuicTest {};

TEST_F(QuicWriteBlockedListTest, PriorityOrder) {
  QuicWriteBlockedList write_blocked_list;

  // Mark streams blocked in roughly reverse priority order, and
  // verify that streams are sorted.
  write_blocked_list.RegisterStream(40, false, kV3LowestPriority);
  write_blocked_list.RegisterStream(23, false, kV3HighestPriority);
  write_blocked_list.RegisterStream(17, false, kV3HighestPriority);
  write_blocked_list.RegisterStream(1, true, kV3HighestPriority);
  write_blocked_list.RegisterStream(3, true, kV3HighestPriority);

  write_blocked_list.AddStream(40);
  EXPECT_TRUE(write_blocked_list.IsStreamBlocked(40));
  write_blocked_list.AddStream(23);
  EXPECT_TRUE(write_blocked_list.IsStreamBlocked(23));
  write_blocked_list.AddStream(17);
  EXPECT_TRUE(write_blocked_list.IsStreamBlocked(17));
  write_blocked_list.AddStream(3);
  EXPECT_TRUE(write_blocked_list.IsStreamBlocked(3));
  write_blocked_list.AddStream(1);
  EXPECT_TRUE(write_blocked_list.IsStreamBlocked(1));

  EXPECT_EQ(5u, write_blocked_list.NumBlockedStreams());
  EXPECT_TRUE(write_blocked_list.HasWriteBlockedSpecialStream());
  EXPECT_EQ(2u, write_blocked_list.NumBlockedSpecialStreams());
  EXPECT_TRUE(write_blocked_list.HasWriteBlockedDataStreams());
  // The Crypto stream is highest priority.
  EXPECT_EQ(1u, write_blocked_list.PopFront());
  EXPECT_EQ(1u, write_blocked_list.NumBlockedSpecialStreams());
  EXPECT_FALSE(write_blocked_list.IsStreamBlocked(1));
  // Followed by the Headers stream.
  EXPECT_EQ(3u, write_blocked_list.PopFront());
  EXPECT_EQ(0u, write_blocked_list.NumBlockedSpecialStreams());
  EXPECT_FALSE(write_blocked_list.IsStreamBlocked(3));
  // Streams with same priority are popped in the order they were inserted.
  EXPECT_EQ(23u, write_blocked_list.PopFront());
  EXPECT_FALSE(write_blocked_list.IsStreamBlocked(23));
  EXPECT_EQ(17u, write_blocked_list.PopFront());
  EXPECT_FALSE(write_blocked_list.IsStreamBlocked(17));
  // Low priority stream appears last.
  EXPECT_EQ(40u, write_blocked_list.PopFront());
  EXPECT_FALSE(write_blocked_list.IsStreamBlocked(40));

  EXPECT_EQ(0u, write_blocked_list.NumBlockedStreams());
  EXPECT_FALSE(write_blocked_list.HasWriteBlockedSpecialStream());
  EXPECT_FALSE(write_blocked_list.HasWriteBlockedDataStreams());
}

TEST_F(QuicWriteBlockedListTest, CryptoStream) {
  QuicWriteBlockedList write_blocked_list;
  write_blocked_list.RegisterStream(1, true, kV3HighestPriority);
  write_blocked_list.AddStream(1);

  EXPECT_EQ(1u, write_blocked_list.NumBlockedStreams());
  EXPECT_TRUE(write_blocked_list.HasWriteBlockedSpecialStream());
  EXPECT_EQ(1u, write_blocked_list.PopFront());
  EXPECT_EQ(0u, write_blocked_list.NumBlockedStreams());
  EXPECT_FALSE(write_blocked_list.HasWriteBlockedSpecialStream());
}

TEST_F(QuicWriteBlockedListTest, HeadersStream) {
  QuicWriteBlockedList write_blocked_list;
  write_blocked_list.RegisterStream(3, true, kV3HighestPriority);
  write_blocked_list.AddStream(3);

  EXPECT_EQ(1u, write_blocked_list.NumBlockedStreams());
  EXPECT_TRUE(write_blocked_list.HasWriteBlockedSpecialStream());
  EXPECT_EQ(3u, write_blocked_list.PopFront());
  EXPECT_EQ(0u, write_blocked_list.NumBlockedStreams());
  EXPECT_FALSE(write_blocked_list.HasWriteBlockedSpecialStream());
}

TEST_F(QuicWriteBlockedListTest, VerifyHeadersStream) {
  QuicWriteBlockedList write_blocked_list;
  write_blocked_list.RegisterStream(5, false, kV3HighestPriority);
  write_blocked_list.RegisterStream(3, true, kV3HighestPriority);
  write_blocked_list.AddStream(5);
  write_blocked_list.AddStream(3);

  EXPECT_EQ(2u, write_blocked_list.NumBlockedStreams());
  EXPECT_TRUE(write_blocked_list.HasWriteBlockedSpecialStream());
  EXPECT_TRUE(write_blocked_list.HasWriteBlockedDataStreams());
  // In newer QUIC versions, there is a headers stream which is
  // higher priority than data streams.
  EXPECT_EQ(3u, write_blocked_list.PopFront());
  EXPECT_EQ(5u, write_blocked_list.PopFront());
  EXPECT_EQ(0u, write_blocked_list.NumBlockedStreams());
  EXPECT_FALSE(write_blocked_list.HasWriteBlockedSpecialStream());
  EXPECT_FALSE(write_blocked_list.HasWriteBlockedDataStreams());
}

TEST_F(QuicWriteBlockedListTest, NoDuplicateEntries) {
  // Test that QuicWriteBlockedList doesn't allow duplicate entries.
  QuicWriteBlockedList write_blocked_list;

  // Try to add a stream to the write blocked list multiple times at the same
  // priority.
  const QuicStreamId kBlockedId = 3 + 2;
  write_blocked_list.RegisterStream(kBlockedId, false, kV3HighestPriority);
  write_blocked_list.AddStream(kBlockedId);
  write_blocked_list.AddStream(kBlockedId);
  write_blocked_list.AddStream(kBlockedId);

  // This should only result in one blocked stream being added.
  EXPECT_EQ(1u, write_blocked_list.NumBlockedStreams());
  EXPECT_TRUE(write_blocked_list.HasWriteBlockedDataStreams());

  // There should only be one stream to pop off the front.
  EXPECT_EQ(kBlockedId, write_blocked_list.PopFront());
  EXPECT_EQ(0u, write_blocked_list.NumBlockedStreams());
  EXPECT_FALSE(write_blocked_list.HasWriteBlockedDataStreams());
}

TEST_F(QuicWriteBlockedListTest, BatchingWrites) {
  QuicWriteBlockedList write_blocked_list;

  const QuicStreamId id1 = 3 + 2;
  const QuicStreamId id2 = id1 + 2;
  const QuicStreamId id3 = id2 + 2;
  write_blocked_list.RegisterStream(id1, false, kV3LowestPriority);
  write_blocked_list.RegisterStream(id2, false, kV3LowestPriority);
  write_blocked_list.RegisterStream(id3, false, kV3HighestPriority);

  write_blocked_list.AddStream(id1);
  write_blocked_list.AddStream(id2);
  EXPECT_EQ(2u, write_blocked_list.NumBlockedStreams());

  // The first stream we push back should stay at the front until 16k is
  // written.
  EXPECT_EQ(id1, write_blocked_list.PopFront());
  write_blocked_list.UpdateBytesForStream(id1, 15999);
  write_blocked_list.AddStream(id1);
  EXPECT_EQ(2u, write_blocked_list.NumBlockedStreams());
  EXPECT_EQ(id1, write_blocked_list.PopFront());

  // Once 16k is written the first stream will yield to the next.
  write_blocked_list.UpdateBytesForStream(id1, 1);
  write_blocked_list.AddStream(id1);
  EXPECT_EQ(2u, write_blocked_list.NumBlockedStreams());
  EXPECT_EQ(id2, write_blocked_list.PopFront());

  // Set the new stream to have written all but one byte.
  write_blocked_list.UpdateBytesForStream(id2, 15999);
  write_blocked_list.AddStream(id2);
  EXPECT_EQ(2u, write_blocked_list.NumBlockedStreams());

  // Ensure higher priority streams are popped first.
  write_blocked_list.AddStream(id3);
  EXPECT_EQ(id3, write_blocked_list.PopFront());

  // Higher priority streams will always be popped first, even if using their
  // byte quota
  write_blocked_list.UpdateBytesForStream(id3, 20000);
  write_blocked_list.AddStream(id3);
  EXPECT_EQ(id3, write_blocked_list.PopFront());

  // Once the higher priority stream is out of the way, id2 will resume its 16k
  // write, with only 1 byte remaining of its guaranteed write allocation.
  EXPECT_EQ(id2, write_blocked_list.PopFront());
  write_blocked_list.UpdateBytesForStream(id2, 1);
  write_blocked_list.AddStream(id2);
  EXPECT_EQ(2u, write_blocked_list.NumBlockedStreams());
  EXPECT_EQ(id1, write_blocked_list.PopFront());
}

TEST_F(QuicWriteBlockedListTest, Ceding) {
  QuicWriteBlockedList write_blocked_list;

  write_blocked_list.RegisterStream(15, false, kV3HighestPriority);
  write_blocked_list.RegisterStream(16, false, kV3HighestPriority);
  write_blocked_list.RegisterStream(5, false, 5);
  write_blocked_list.RegisterStream(4, false, 5);
  write_blocked_list.RegisterStream(7, false, 7);
  write_blocked_list.RegisterStream(1, true, kV3HighestPriority);
  write_blocked_list.RegisterStream(3, true, kV3HighestPriority);

  // When nothing is on the list, nothing yields.
  EXPECT_FALSE(write_blocked_list.ShouldYield(5));

  write_blocked_list.AddStream(5);
  // 5 should not yield to itself.
  EXPECT_FALSE(write_blocked_list.ShouldYield(5));
  // 4 and 7 are equal or lower priority and should yield to 5.
  EXPECT_TRUE(write_blocked_list.ShouldYield(4));
  EXPECT_TRUE(write_blocked_list.ShouldYield(7));
  // 15, headers and crypto should preempt 5.
  EXPECT_FALSE(write_blocked_list.ShouldYield(15));
  EXPECT_FALSE(write_blocked_list.ShouldYield(3));
  EXPECT_FALSE(write_blocked_list.ShouldYield(1));

  // Block a high priority stream.
  write_blocked_list.AddStream(15);
  // 16 should yield (same priority) but headers and crypto will still not.
  EXPECT_TRUE(write_blocked_list.ShouldYield(16));
  EXPECT_FALSE(write_blocked_list.ShouldYield(3));
  EXPECT_FALSE(write_blocked_list.ShouldYield(1));

  // Block the headers stream.  All streams but crypto and headers should yield.
  write_blocked_list.AddStream(3);
  EXPECT_TRUE(write_blocked_list.ShouldYield(16));
  EXPECT_TRUE(write_blocked_list.ShouldYield(15));
  EXPECT_FALSE(write_blocked_list.ShouldYield(3));
  EXPECT_FALSE(write_blocked_list.ShouldYield(1));

  // Block the crypto stream.  All streams but crypto should yield.
  write_blocked_list.AddStream(1);
  EXPECT_TRUE(write_blocked_list.ShouldYield(16));
  EXPECT_TRUE(write_blocked_list.ShouldYield(15));
  EXPECT_TRUE(write_blocked_list.ShouldYield(3));
  EXPECT_FALSE(write_blocked_list.ShouldYield(1));
}

}  // namespace
}  // namespace test
}  // namespace quic
