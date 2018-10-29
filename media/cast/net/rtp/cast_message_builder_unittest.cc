// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtp/cast_message_builder.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/cast/net/rtcp/rtcp_defines.h"
#include "media/cast/net/rtp/framer.h"
#include "media/cast/net/rtp/rtp_defines.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

namespace {
static const uint32_t kSsrc = 0x1234;
static const uint32_t kShortTimeIncrementMs = 10;
static const uint32_t kLongTimeIncrementMs = 40;
static const int64_t kStartMillisecond = INT64_C(12345678900000);

typedef std::map<FrameId, size_t> MissingPacketsMap;

class NackFeedbackVerification : public RtpPayloadFeedback {
 public:
  NackFeedbackVerification() : triggered_(false) {}

  void CastFeedback(const RtcpCastMessage& cast_feedback) final {
    EXPECT_EQ(kSsrc, cast_feedback.remote_ssrc);

    last_frame_acked_ = cast_feedback.ack_frame_id;

    auto frame_it = cast_feedback.missing_frames_and_packets.begin();

    // Keep track of the number of missing packets per frame.
    missing_packets_.clear();
    while (frame_it != cast_feedback.missing_frames_and_packets.end()) {
      // Check for complete frame lost.
      if ((frame_it->second.size() == 1) &&
          (*frame_it->second.begin() == kRtcpCastAllPacketsLost)) {
        missing_packets_.insert(
            std::make_pair(frame_it->first, kRtcpCastAllPacketsLost));
      } else {
        missing_packets_.insert(
            std::make_pair(frame_it->first, frame_it->second.size()));
      }
      ++frame_it;
    }
    triggered_ = true;
  }

  size_t num_missing_packets(FrameId frame_id) {
    MissingPacketsMap::iterator it;
    it = missing_packets_.find(frame_id);
    if (it == missing_packets_.end())
      return 0;

    return it->second;
  }

  // Holds value for one call.
  bool triggered() {
    bool ret_val = triggered_;
    triggered_ = false;
    return ret_val;
  }

  FrameId last_frame_acked() { return last_frame_acked_; }

 private:
  bool triggered_;
  MissingPacketsMap missing_packets_;  // Missing packets per frame.
  FrameId last_frame_acked_;

  DISALLOW_COPY_AND_ASSIGN(NackFeedbackVerification);
};
}  // namespace

class CastMessageBuilderTest : public ::testing::Test {
 protected:
  CastMessageBuilderTest()
      : framer_(&testing_clock_,
                &feedback_,
                kSsrc,
                true,
                10),
        cast_msg_builder_(new CastMessageBuilder(&testing_clock_,
                                                 &feedback_,
                                                 &framer_,
                                                 kSsrc,
                                                 true,
                                                 0)) {
    rtp_header_.sender_ssrc = kSsrc;
    rtp_header_.is_key_frame = false;
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
  }

  ~CastMessageBuilderTest() override = default;

  void SetFrameIds(FrameId frame_id, FrameId reference_frame_id) {
    rtp_header_.frame_id = frame_id;
    rtp_header_.reference_frame_id = reference_frame_id;
  }

  void SetPacketId(uint16_t packet_id) { rtp_header_.packet_id = packet_id; }

  void SetMaxPacketId(uint16_t max_packet_id) {
    rtp_header_.max_packet_id = max_packet_id;
  }

  void SetKeyFrame(bool is_key) { rtp_header_.is_key_frame = is_key; }

  void InsertPacket() {
    bool duplicate;
    uint8_t payload = 0;
    if (framer_.InsertPacket(&payload, 1, rtp_header_, &duplicate)) {
      cast_msg_builder_->CompleteFrameReceived(rtp_header_.frame_id);
    }
    cast_msg_builder_->UpdateCastMessage();
  }

  void SetDecoderSlowerThanMaxFrameRate(int max_unacked_frames) {
    cast_msg_builder_.reset(new CastMessageBuilder(&testing_clock_,
                                                   &feedback_,
                                                   &framer_,
                                                   kSsrc,
                                                   false,
                                                   max_unacked_frames));
  }

  NackFeedbackVerification feedback_;
  Framer framer_;
  std::unique_ptr<CastMessageBuilder> cast_msg_builder_;
  RtpCastHeader rtp_header_;
  base::SimpleTestTickClock testing_clock_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastMessageBuilderTest);
};

TEST_F(CastMessageBuilderTest, OneFrameNackList) {
  SetFrameIds(FrameId::first(), FrameId::first());
  SetPacketId(4);
  SetMaxPacketId(10);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  EXPECT_FALSE(feedback_.triggered());
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetPacketId(5);
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(4u, feedback_.num_missing_packets(FrameId::first()));
}

TEST_F(CastMessageBuilderTest, CompleteFrameMissing) {
  SetFrameIds(FrameId::first(), FrameId::first());
  SetPacketId(2);
  SetMaxPacketId(5);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetFrameIds(FrameId::first() + 2, FrameId::first() + 1);
  SetPacketId(2);
  SetMaxPacketId(5);
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(kRtcpCastAllPacketsLost,
            feedback_.num_missing_packets(FrameId::first() + 1));
}

TEST_F(CastMessageBuilderTest, ReleaseFrames) {
  SetFrameIds(FrameId::first() + 1, FrameId::first());
  SetPacketId(0);
  SetMaxPacketId(1);
  InsertPacket();
  EXPECT_FALSE(feedback_.triggered());
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetFrameIds(FrameId::first() + 2, FrameId::first() + 1);
  SetPacketId(0);
  SetMaxPacketId(0);
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetFrameIds(FrameId::first() + 3, FrameId::first() + 2);
  SetPacketId(0);
  SetMaxPacketId(5);
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(FrameId::first() + 2, feedback_.last_frame_acked());
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetFrameIds(FrameId::first() + 5, FrameId::first() + 5);
  SetPacketId(0);
  SetMaxPacketId(0);
  SetKeyFrame(true);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  // Simulate 5 being pulled for rendering.
  framer_.ReleaseFrame(FrameId::first() + 5);
  cast_msg_builder_->UpdateCastMessage();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(FrameId::first() + 5, feedback_.last_frame_acked());
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  SetFrameIds(FrameId::first() + 1, FrameId::first());
  SetPacketId(1);
  SetMaxPacketId(1);
  InsertPacket();
  EXPECT_FALSE(feedback_.triggered());
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(FrameId::first() + 5, feedback_.last_frame_acked());
}

TEST_F(CastMessageBuilderTest, NackUntilMaxReceivedPacket) {
  SetFrameIds(FrameId::first(), FrameId::first());
  SetPacketId(0);
  SetMaxPacketId(20);
  SetKeyFrame(true);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetPacketId(5);
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(4u, feedback_.num_missing_packets(FrameId::first()));
}

TEST_F(CastMessageBuilderTest, NackUntilMaxReceivedPacketNextFrame) {
  SetFrameIds(FrameId::first(), FrameId::first());
  SetPacketId(0);
  SetMaxPacketId(20);
  SetKeyFrame(true);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetPacketId(5);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(4u, feedback_.num_missing_packets(FrameId::first()));
  SetFrameIds(FrameId::first() + 1, FrameId::first());
  SetMaxPacketId(2);
  SetPacketId(0);
  SetKeyFrame(false);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(19u, feedback_.num_missing_packets(FrameId::first()));
}

TEST_F(CastMessageBuilderTest, NackUntilMaxReceivedPacketNextKey) {
  SetFrameIds(FrameId::first(), FrameId::first());
  SetPacketId(0);
  SetMaxPacketId(20);
  SetKeyFrame(true);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  SetPacketId(5);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(4u, feedback_.num_missing_packets(FrameId::first()));
  SetFrameIds(FrameId::first() + 1, FrameId::first() + 1);
  SetMaxPacketId(0);
  SetPacketId(0);
  SetKeyFrame(true);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(0u, feedback_.num_missing_packets(FrameId::first()));
}

TEST_F(CastMessageBuilderTest, BasicRps) {
  SetFrameIds(FrameId::first(), FrameId::first());
  SetPacketId(0);
  SetMaxPacketId(0);
  SetKeyFrame(true);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(FrameId::first(), feedback_.last_frame_acked());
  SetFrameIds(FrameId::first() + 3, FrameId::first());
  SetKeyFrame(false);
  InsertPacket();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(FrameId::first() + 3, feedback_.last_frame_acked());
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kLongTimeIncrementMs));
  // Simulate 3 being pulled for rendering.
  framer_.ReleaseFrame(FrameId::first() + 3);
  cast_msg_builder_->UpdateCastMessage();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(FrameId::first() + 3, feedback_.last_frame_acked());
}

TEST_F(CastMessageBuilderTest, InOrderRps) {
  // Create a pattern - skip to rps, and don't look back.
  SetFrameIds(FrameId::first(), FrameId::first());
  SetPacketId(0);
  SetMaxPacketId(0);
  SetKeyFrame(true);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(FrameId::first(), feedback_.last_frame_acked());
  SetFrameIds(FrameId::first() + 1, FrameId::first());
  SetPacketId(0);
  SetMaxPacketId(1);
  SetKeyFrame(false);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  EXPECT_FALSE(feedback_.triggered());
  SetFrameIds(FrameId::first() + 3, FrameId::first());
  SetPacketId(0);
  SetMaxPacketId(0);
  SetKeyFrame(false);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  // Simulate 3 being pulled for rendering.
  framer_.ReleaseFrame(FrameId::first() + 3);
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  cast_msg_builder_->UpdateCastMessage();
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(FrameId::first() + 3, feedback_.last_frame_acked());
  // Make an old frame complete - should not trigger an ack.
  SetFrameIds(FrameId::first() + 1, FrameId::first());
  SetPacketId(1);
  SetMaxPacketId(1);
  SetKeyFrame(false);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  EXPECT_FALSE(feedback_.triggered());
  EXPECT_EQ(FrameId::first() + 3, feedback_.last_frame_acked());
}

TEST_F(CastMessageBuilderTest, SlowDownAck) {
  SetDecoderSlowerThanMaxFrameRate(3);
  SetFrameIds(FrameId::first(), FrameId::first());
  SetPacketId(0);
  SetMaxPacketId(0);
  SetKeyFrame(true);
  InsertPacket();

  FrameId frame_id;
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  SetKeyFrame(false);
  for (frame_id = FrameId::first() + 1; frame_id < FrameId::first() + 3;
       ++frame_id) {
    EXPECT_TRUE(feedback_.triggered());
    EXPECT_EQ(frame_id - 1, feedback_.last_frame_acked());
    SetFrameIds(frame_id, frame_id - 1);
    InsertPacket();
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  }
  // We should now have entered the slowdown ACK state.
  FrameId expected_frame_id = FrameId::first() + 1;
  for (; frame_id < FrameId::first() + 10; ++frame_id) {
    if ((frame_id - FrameId::first()) % 2) {
      ++expected_frame_id;
      EXPECT_TRUE(feedback_.triggered());
    } else {
      EXPECT_FALSE(feedback_.triggered());
    }
    EXPECT_EQ(expected_frame_id, feedback_.last_frame_acked());
    SetFrameIds(frame_id, frame_id - 1);
    InsertPacket();
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  }
  EXPECT_FALSE(feedback_.triggered());
  EXPECT_EQ(expected_frame_id, feedback_.last_frame_acked());

  // Simulate frame_id being pulled for rendering.
  framer_.ReleaseFrame(frame_id);
  // We should now leave the slowdown ACK state.
  ++frame_id;
  SetFrameIds(frame_id, frame_id - 1);
  InsertPacket();
  testing_clock_.Advance(
      base::TimeDelta::FromMilliseconds(kShortTimeIncrementMs));
  EXPECT_TRUE(feedback_.triggered());
  EXPECT_EQ(frame_id, feedback_.last_frame_acked());
}

}  // namespace cast
}  // namespace media
