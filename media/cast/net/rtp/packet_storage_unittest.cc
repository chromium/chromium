// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtp/packet_storage.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <vector>

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "media/cast/constants.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

static const size_t kStoredFrames = 10;

// Generate |number_of_frames| and store into |*storage|.
// First frame has 1 packet, second frame has 2 packets, etc.
static void StoreFrames(int number_of_frames,
                        FrameId first_frame_id,
                        PacketStorage* storage) {
  const int kSsrc = 1;
  for (int i = 0; i < number_of_frames; ++i) {
    SendPacketVector packets;
    // First frame has 1 packet, second frame has 2 packets, etc.
    const int number_of_packets = i + 1;
    for (int j = 0; j < number_of_packets; ++j) {
      Packet test_packet(1, 0);
      packets.push_back(
          std::make_pair(PacketKey(base::TimeTicks(), kSsrc, first_frame_id + i,
                                   base::checked_cast<uint16_t>(j)),
                         new base::RefCountedData<Packet>(test_packet)));
    }
    storage->StoreFrame(first_frame_id, packets);
    ++first_frame_id;
  }
}

TEST(PacketStorageTest, NumberOfStoredFrames) {
  PacketStorage storage;

  // Use a big frame ID to make sure none of the offset calculations are
  // overflowing.
  const FrameId frame_id = FrameId::first() +
                           std::numeric_limits<int32_t>::max() +
                           std::numeric_limits<int32_t>::max();
  StoreFrames(kMaxUnackedFrames / 2, frame_id, &storage);
  EXPECT_EQ(static_cast<size_t>(kMaxUnackedFrames / 2),
            storage.GetNumberOfStoredFrames());
}

TEST(PacketStorageTest, StoreAndGetFrames) {
  PacketStorage storage;

  // Use a big frame ID to make sure none of the offset calculations are
  // overflowing.
  const FrameId first_frame_id = FrameId::first() +
                                 std::numeric_limits<int32_t>::max() +
                                 std::numeric_limits<int32_t>::max();

  StoreFrames(kStoredFrames, first_frame_id, &storage);
  EXPECT_EQ(std::min<size_t>(kMaxUnackedFrames, kStoredFrames),
            storage.GetNumberOfStoredFrames());

  // Expect we get the correct frames by looking at the number of
  // packets.
  for (size_t i = 0; i < kStoredFrames; ++i) {
    ASSERT_TRUE(storage.GetFramePackets(first_frame_id + i));
    EXPECT_EQ(i + 1, storage.GetFramePackets(first_frame_id + i)->size());
  }

  // Expect not to see packets for frames not stored.
  ASSERT_FALSE(storage.GetFramePackets(first_frame_id - 1));
  ASSERT_FALSE(storage.GetFramePackets(first_frame_id + kStoredFrames + 1));
}

TEST(PacketStorageTest, FramesReleased) {
  PacketStorage storage;

  const FrameId first_frame_id = FrameId::first();
  StoreFrames(5, first_frame_id, &storage);
  EXPECT_EQ(std::min<size_t>(kMaxUnackedFrames, 5),
            storage.GetNumberOfStoredFrames());

  for (FrameId frame_id = first_frame_id; frame_id < first_frame_id + 5;
       ++frame_id) {
    EXPECT_TRUE(storage.GetFramePackets(frame_id));
  }

  storage.ReleaseFrame(first_frame_id + 2);
  EXPECT_EQ(4u, storage.GetNumberOfStoredFrames());
  EXPECT_FALSE(storage.GetFramePackets(first_frame_id + 2));

  storage.ReleaseFrame(first_frame_id + 0);
  EXPECT_EQ(3u, storage.GetNumberOfStoredFrames());
  EXPECT_FALSE(storage.GetFramePackets(first_frame_id + 0));

  storage.ReleaseFrame(first_frame_id + 3);
  EXPECT_EQ(2u, storage.GetNumberOfStoredFrames());
  EXPECT_FALSE(storage.GetFramePackets(first_frame_id + 3));

  storage.ReleaseFrame(first_frame_id + 4);
  EXPECT_EQ(1u, storage.GetNumberOfStoredFrames());
  EXPECT_FALSE(storage.GetFramePackets(first_frame_id + 4));

  storage.ReleaseFrame(first_frame_id + 1);
  EXPECT_EQ(0u, storage.GetNumberOfStoredFrames());
  EXPECT_FALSE(storage.GetFramePackets(first_frame_id + 1));
}

}  // namespace cast
}  // namespace media
