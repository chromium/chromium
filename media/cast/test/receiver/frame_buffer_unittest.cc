// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "media/cast/common/encoded_frame.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/test/receiver/frame_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/encoded_frame.h"

namespace media {
namespace cast {

class FrameBufferTest : public ::testing::Test {
 public:
  FrameBufferTest(const FrameBufferTest&) = delete;
  FrameBufferTest& operator=(const FrameBufferTest&) = delete;

 protected:
  FrameBufferTest() {
    payload_.assign(kMaxIpPacketSize, 0);
    rtp_header_.frame_id = FrameId::first();
    rtp_header_.reference_frame_id = FrameId::first();
  }

  ~FrameBufferTest() override = default;

  FrameBuffer buffer_;
  std::vector<uint8_t> payload_;
  RtpCastHeader rtp_header_;
};

TEST_F(FrameBufferTest, OnePacketInsertSanity) {
  rtp_header_.rtp_timestamp = RtpTimeTicks().Expand(UINT32_C(3000));
  rtp_header_.is_key_frame = true;
  rtp_header_.frame_id = FrameId::first() + 5;
  rtp_header_.reference_frame_id = FrameId::first() + 5;
  buffer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_);
  EncodedFrame frame;
  EXPECT_TRUE(buffer_.AssembleEncodedFrame(&frame));
  EXPECT_EQ(openscreen::cast::EncodedFrame::Dependency::kKeyFrame,
            frame.dependency);
  EXPECT_EQ(FrameId::first() + 5, frame.frame_id);
  EXPECT_EQ(FrameId::first() + 5, frame.referenced_frame_id);
  EXPECT_EQ(3000u, frame.rtp_timestamp.lower_32_bits());
}

TEST_F(FrameBufferTest, EmptyBuffer) {
  EXPECT_FALSE(buffer_.Complete());
  EncodedFrame frame;
  EXPECT_FALSE(buffer_.AssembleEncodedFrame(&frame));
}

TEST_F(FrameBufferTest, DefaultOnePacketFrame) {
  buffer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_);
  EXPECT_TRUE(buffer_.Complete());
  EXPECT_FALSE(buffer_.is_key_frame());
  EncodedFrame frame;
  EXPECT_TRUE(buffer_.AssembleEncodedFrame(&frame));
  EXPECT_EQ(payload_.size(), frame.data.size());
}

TEST_F(FrameBufferTest, MultiplePacketFrame) {
  rtp_header_.is_key_frame = true;
  rtp_header_.max_packet_id = 2;
  buffer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_);
  ++rtp_header_.packet_id;
  buffer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_);
  ++rtp_header_.packet_id;
  buffer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_);
  ++rtp_header_.packet_id;
  EXPECT_TRUE(buffer_.Complete());
  EXPECT_TRUE(buffer_.is_key_frame());
  EncodedFrame frame;
  EXPECT_TRUE(buffer_.AssembleEncodedFrame(&frame));
  EXPECT_EQ(3 * payload_.size(), frame.data.size());
}

TEST_F(FrameBufferTest, IncompleteFrame) {
  rtp_header_.max_packet_id = 4;
  buffer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_);
  ++rtp_header_.packet_id;
  buffer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_);
  ++rtp_header_.packet_id;
  // Increment again - skip packet #2.
  ++rtp_header_.packet_id;
  buffer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_);
  ++rtp_header_.packet_id;
  buffer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_);
  EXPECT_FALSE(buffer_.Complete());
  // Insert missing packet.
  rtp_header_.packet_id = 2;
  buffer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_);
  EXPECT_TRUE(buffer_.Complete());
}

}  // namespace cast
}  // namespace media
