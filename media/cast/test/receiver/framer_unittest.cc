// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/test/simple_test_tick_clock.h"
#include "media/cast/common/encoded_frame.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/test/mock_rtp_payload_feedback.h"
#include "media/cast/test/receiver/framer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/encoded_frame.h"

using Dependency = openscreen::cast::EncodedFrame::Dependency;

namespace media {
namespace cast {

class FramerTest : public ::testing::Test {
 public:
  FramerTest(const FramerTest&) = delete;
  FramerTest& operator=(const FramerTest&) = delete;

 protected:
  FramerTest()
      : mock_rtp_payload_feedback_(),
        framer_(&testing_clock_, &mock_rtp_payload_feedback_, 0, true, 0) {
    payload_.assign(kMaxIpPacketSize, 0);

    EXPECT_CALL(mock_rtp_payload_feedback_, CastFeedback(testing::_))
        .WillRepeatedly(testing::Return());
  }

  ~FramerTest() override = default;

  std::vector<uint8_t> payload_;
  RtpCastHeader rtp_header_;
  MockRtpPayloadFeedback mock_rtp_payload_feedback_;
  Framer framer_;
  base::SimpleTestTickClock testing_clock_;
};

TEST_F(FramerTest, EmptyState) {
  EncodedFrame frame;
  bool next_frame = false;
  bool multiple = false;
  EXPECT_FALSE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
}

TEST_F(FramerTest, AlwaysStartWithKey) {
  EncodedFrame frame;
  bool next_frame = false;
  bool complete = false;
  bool multiple = false;
  bool duplicate = false;

  // Insert non key first frame.
  complete = framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_,
                                  &duplicate);
  EXPECT_TRUE(complete);
  EXPECT_FALSE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  rtp_header_.frame_id = FrameId::first() + 1;
  rtp_header_.reference_frame_id = FrameId::first() + 1;
  rtp_header_.is_key_frame = true;
  complete = framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_,
                                  &duplicate);
  EXPECT_TRUE(complete);
  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_TRUE(next_frame);
  EXPECT_TRUE(multiple);
  EXPECT_EQ(Dependency::kKeyFrame, frame.dependency);
  EXPECT_EQ(FrameId::first() + 1, frame.frame_id);
  EXPECT_EQ(FrameId::first() + 1, frame.referenced_frame_id);
  framer_.ReleaseFrame(frame.frame_id);
}

TEST_F(FramerTest, CompleteFrame) {
  EncodedFrame frame;
  bool next_frame = false;
  bool complete = false;
  bool multiple = false;
  bool duplicate = false;

  // Start with a complete key frame.
  rtp_header_.is_key_frame = true;
  rtp_header_.frame_id = FrameId::first();
  rtp_header_.reference_frame_id = FrameId::first();
  complete = framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_,
                                  &duplicate);
  EXPECT_TRUE(complete);
  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_TRUE(next_frame);
  EXPECT_FALSE(multiple);
  EXPECT_EQ(Dependency::kKeyFrame, frame.dependency);
  EXPECT_EQ(FrameId::first(), frame.frame_id);
  EXPECT_EQ(FrameId::first(), frame.referenced_frame_id);
  framer_.ReleaseFrame(frame.frame_id);

  // Incomplete delta.
  ++rtp_header_.frame_id;
  rtp_header_.reference_frame_id = rtp_header_.frame_id - 1;
  rtp_header_.is_key_frame = false;
  rtp_header_.max_packet_id = 2;
  complete = framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_,
                                  &duplicate);
  EXPECT_FALSE(complete);
  EXPECT_FALSE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));

  // Complete delta - can't skip, as incomplete sequence.
  ++rtp_header_.frame_id;
  rtp_header_.reference_frame_id = rtp_header_.frame_id - 1;
  rtp_header_.max_packet_id = 0;
  complete = framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_,
                                  &duplicate);
  EXPECT_TRUE(complete);
  EXPECT_FALSE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
}

TEST_F(FramerTest, DuplicatePackets) {
  EncodedFrame frame;
  bool next_frame = false;
  bool complete = false;
  bool multiple = false;
  bool duplicate = false;

  // Start with an incomplete key frame.
  rtp_header_.is_key_frame = true;
  rtp_header_.frame_id = FrameId::first();
  rtp_header_.reference_frame_id = FrameId::first();
  rtp_header_.max_packet_id = 1;
  duplicate = true;
  complete = framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_,
                                  &duplicate);
  EXPECT_FALSE(complete);
  EXPECT_FALSE(duplicate);
  EXPECT_FALSE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));

  // Add same packet again in incomplete key frame.
  duplicate = false;
  complete = framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_,
                                  &duplicate);
  EXPECT_FALSE(complete);
  EXPECT_TRUE(duplicate);
  EXPECT_FALSE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));

  // Complete key frame.
  rtp_header_.packet_id = 1;
  duplicate = true;
  complete = framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_,
                                  &duplicate);
  EXPECT_TRUE(complete);
  EXPECT_FALSE(duplicate);
  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_EQ(Dependency::kKeyFrame, frame.dependency);
  EXPECT_FALSE(multiple);
  EXPECT_EQ(FrameId::first(), frame.referenced_frame_id);

  // Add same packet again in complete key frame.
  duplicate = false;
  complete = framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_,
                                  &duplicate);
  EXPECT_FALSE(complete);
  EXPECT_TRUE(duplicate);
  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_EQ(Dependency::kKeyFrame, frame.dependency);
  EXPECT_EQ(FrameId::first(), frame.frame_id);
  EXPECT_FALSE(multiple);
  EXPECT_EQ(FrameId::first(), frame.referenced_frame_id);
  framer_.ReleaseFrame(frame.frame_id);

  // Incomplete delta frame.
  ++rtp_header_.frame_id;
  rtp_header_.reference_frame_id = rtp_header_.frame_id - 1;
  rtp_header_.packet_id = 0;
  rtp_header_.is_key_frame = false;
  duplicate = true;
  complete = framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_,
                                  &duplicate);
  EXPECT_FALSE(complete);
  EXPECT_FALSE(duplicate);
  EXPECT_FALSE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));

  // Add same packet again in incomplete delta frame.
  duplicate = false;
  complete = framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_,
                                  &duplicate);
  EXPECT_FALSE(complete);
  EXPECT_TRUE(duplicate);
  EXPECT_FALSE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));

  // Complete delta frame.
  rtp_header_.packet_id = 1;
  duplicate = true;
  complete = framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_,
                                  &duplicate);
  EXPECT_TRUE(complete);
  EXPECT_FALSE(duplicate);
  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_EQ(Dependency::kDependent, frame.dependency);
  EXPECT_EQ(FrameId::first() + 1, frame.frame_id);
  EXPECT_EQ(FrameId::first(), frame.referenced_frame_id);
  EXPECT_FALSE(multiple);

  // Add same packet again in complete delta frame.
  duplicate = false;
  complete = framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_,
                                  &duplicate);
  EXPECT_FALSE(complete);
  EXPECT_TRUE(duplicate);
  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_EQ(Dependency::kDependent, frame.dependency);
  EXPECT_EQ(FrameId::first() + 1, frame.frame_id);
  EXPECT_EQ(FrameId::first(), frame.referenced_frame_id);
  EXPECT_FALSE(multiple);
}

TEST_F(FramerTest, ContinuousSequence) {
  EncodedFrame frame;
  bool next_frame = false;
  bool complete = false;
  bool multiple = false;
  bool duplicate = false;

  // Start with a complete key frame.
  rtp_header_.is_key_frame = true;
  rtp_header_.frame_id = FrameId::first();
  rtp_header_.reference_frame_id = FrameId::first();
  complete = framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_,
                                  &duplicate);
  EXPECT_TRUE(complete);
  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_TRUE(next_frame);
  EXPECT_FALSE(multiple);
  EXPECT_EQ(Dependency::kKeyFrame, frame.dependency);
  EXPECT_EQ(FrameId::first(), frame.frame_id);
  EXPECT_EQ(FrameId::first(), frame.referenced_frame_id);
  framer_.ReleaseFrame(frame.frame_id);

  // Complete - not continuous.
  rtp_header_.frame_id = FrameId::first() + 2;
  rtp_header_.reference_frame_id = rtp_header_.frame_id - 1;
  rtp_header_.is_key_frame = false;
  complete = framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_,
                                  &duplicate);
  EXPECT_TRUE(complete);
  EXPECT_FALSE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
}

TEST_F(FramerTest, RequireKeyFrameForFirstFrame) {
  EncodedFrame frame;
  bool next_frame = false;
  bool multiple = false;
  bool duplicate = false;

  // Start with a complete key frame.
  rtp_header_.is_key_frame = false;
  rtp_header_.frame_id = FrameId::first();
  framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_, &duplicate);
  EXPECT_FALSE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  rtp_header_.frame_id = FrameId::first() + 1;
  rtp_header_.reference_frame_id = FrameId::first() + 1;
  rtp_header_.is_key_frame = true;
  framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_TRUE(next_frame);
  EXPECT_TRUE(multiple);
}

TEST_F(FramerTest, BasicNonLastReferenceId) {
  EncodedFrame frame;
  bool next_frame = false;
  bool multiple = false;
  bool duplicate = false;

  rtp_header_.is_key_frame = true;
  rtp_header_.frame_id = FrameId::first();
  rtp_header_.reference_frame_id = FrameId::first();
  framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_, &duplicate);

  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_FALSE(multiple);
  framer_.ReleaseFrame(frame.frame_id);

  rtp_header_.is_key_frame = false;
  rtp_header_.frame_id = FrameId::first() + 5;
  rtp_header_.reference_frame_id = FrameId::first();
  framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_, &duplicate);

  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_FALSE(next_frame);
  EXPECT_FALSE(multiple);
}

TEST_F(FramerTest, InOrderReferenceFrameSelection) {
  // Create pattern: 0, 1, 4, 5.
  EncodedFrame frame;
  bool next_frame = false;
  bool multiple = false;
  bool duplicate = false;

  rtp_header_.is_key_frame = true;
  rtp_header_.frame_id = FrameId::first();
  rtp_header_.reference_frame_id = FrameId::first();
  framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_, &duplicate);
  rtp_header_.is_key_frame = false;
  rtp_header_.frame_id = FrameId::first() + 1;
  framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_, &duplicate);

  // Insert frame #2 partially.
  rtp_header_.frame_id = FrameId::first() + 2;
  rtp_header_.max_packet_id = 1;
  framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_, &duplicate);
  rtp_header_.frame_id = FrameId::first() + 4;
  rtp_header_.max_packet_id = 0;
  rtp_header_.reference_frame_id = FrameId::first();
  framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_EQ(Dependency::kKeyFrame, frame.dependency);
  EXPECT_EQ(FrameId::first(), frame.frame_id);
  EXPECT_EQ(FrameId::first(), frame.referenced_frame_id);
  EXPECT_FALSE(multiple);
  framer_.ReleaseFrame(frame.frame_id);
  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_TRUE(next_frame);
  EXPECT_TRUE(multiple);
  EXPECT_EQ(Dependency::kDependent, frame.dependency);
  EXPECT_EQ(FrameId::first() + 1, frame.frame_id);
  EXPECT_EQ(FrameId::first(), frame.referenced_frame_id);
  framer_.ReleaseFrame(frame.frame_id);
  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_FALSE(next_frame);
  EXPECT_FALSE(multiple);
  EXPECT_EQ(Dependency::kDependent, frame.dependency);
  EXPECT_EQ(FrameId::first() + 4, frame.frame_id);
  EXPECT_EQ(FrameId::first(), frame.referenced_frame_id);
  framer_.ReleaseFrame(frame.frame_id);
  // Insert remaining packet of frame #2 - should no be continuous.
  rtp_header_.frame_id = FrameId::first() + 2;
  rtp_header_.packet_id = 1;
  framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_, &duplicate);
  EXPECT_FALSE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  rtp_header_.frame_id = FrameId::first() + 5;
  rtp_header_.reference_frame_id = rtp_header_.frame_id - 1;
  rtp_header_.packet_id = 0;
  rtp_header_.max_packet_id = 0;
  framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_TRUE(next_frame);
  EXPECT_FALSE(multiple);
  EXPECT_EQ(Dependency::kDependent, frame.dependency);
  EXPECT_EQ(FrameId::first() + 5, frame.frame_id);
  EXPECT_EQ(FrameId::first() + 4, frame.referenced_frame_id);
}

TEST_F(FramerTest, ReleasesAllReceivedKeyFramesInContinuousSequence) {
  EncodedFrame frame;
  bool next_frame = false;
  bool multiple = false;
  bool duplicate = false;

  rtp_header_.is_key_frame = true;
  rtp_header_.frame_id = FrameId::first() + 254;
  rtp_header_.reference_frame_id = FrameId::first() + 254;

  framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_TRUE(next_frame);
  EXPECT_FALSE(multiple);
  EXPECT_EQ(Dependency::kKeyFrame, frame.dependency);
  EXPECT_EQ(FrameId::first() + 254, frame.frame_id);
  EXPECT_EQ(FrameId::first() + 254, frame.referenced_frame_id);
  framer_.ReleaseFrame(frame.frame_id);

  rtp_header_.frame_id = FrameId::first() + 255;
  rtp_header_.reference_frame_id = FrameId::first() + 255;
  framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_, &duplicate);

  // Insert wrapped frame - should be continuous.
  rtp_header_.frame_id = FrameId::first() + 256;
  rtp_header_.reference_frame_id = FrameId::first() + 256;
  framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_, &duplicate);

  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_TRUE(next_frame);
  EXPECT_TRUE(multiple);
  EXPECT_EQ(Dependency::kKeyFrame, frame.dependency);
  EXPECT_EQ(FrameId::first() + 255, frame.frame_id);
  EXPECT_EQ(FrameId::first() + 255, frame.referenced_frame_id);
  framer_.ReleaseFrame(frame.frame_id);

  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_TRUE(next_frame);
  EXPECT_FALSE(multiple);
  EXPECT_EQ(Dependency::kKeyFrame, frame.dependency);
  EXPECT_EQ(FrameId::first() + 256, frame.frame_id);
  EXPECT_EQ(FrameId::first() + 256, frame.referenced_frame_id);
  framer_.ReleaseFrame(frame.frame_id);
}

TEST_F(FramerTest, SkipsMissingFramesWhenLaterKeyFramesAreAvailable) {
  EncodedFrame frame;
  bool next_frame = false;
  bool multiple = true;
  bool duplicate = false;

  // Insert and get first frame's packet.
  rtp_header_.is_key_frame = true;
  rtp_header_.frame_id = FrameId::first() + 253;
  rtp_header_.reference_frame_id = FrameId::first() + 253;
  framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_, &duplicate);
  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_TRUE(next_frame);
  EXPECT_FALSE(multiple);
  EXPECT_EQ(Dependency::kKeyFrame, frame.dependency);
  EXPECT_EQ(FrameId::first() + 253, frame.frame_id);
  EXPECT_EQ(FrameId::first() + 253, frame.referenced_frame_id);
  framer_.ReleaseFrame(frame.frame_id);

  // Insert third and fourth frames' packet.
  rtp_header_.frame_id = FrameId::first() + 255;
  rtp_header_.reference_frame_id = FrameId::first() + 255;
  framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_, &duplicate);
  rtp_header_.frame_id = FrameId::first() + 256;
  rtp_header_.reference_frame_id = FrameId::first() + 256;
  framer_.InsertPacket(&payload_[0], payload_.size(), rtp_header_, &duplicate);

  // Get third and fourth frame.
  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_FALSE(next_frame);
  EXPECT_TRUE(multiple);
  EXPECT_EQ(Dependency::kKeyFrame, frame.dependency);
  EXPECT_EQ(FrameId::first() + 255, frame.frame_id);
  EXPECT_EQ(FrameId::first() + 255, frame.referenced_frame_id);
  framer_.ReleaseFrame(frame.frame_id);
  EXPECT_TRUE(framer_.GetEncodedFrame(&frame, &next_frame, &multiple));
  EXPECT_TRUE(next_frame);
  EXPECT_FALSE(multiple);
  EXPECT_EQ(Dependency::kKeyFrame, frame.dependency);
  EXPECT_EQ(FrameId::first() + 256, frame.frame_id);
  EXPECT_EQ(FrameId::first() + 256, frame.referenced_frame_id);
  framer_.ReleaseFrame(frame.frame_id);
}

}  // namespace cast
}  // namespace media
