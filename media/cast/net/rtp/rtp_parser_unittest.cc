// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtp/rtp_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/rand_util.h"
#include "media/cast/net/rtp/rtp_defines.h"
#include "media/cast/test/rtp_packet_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

static const size_t kPacketLength = 1500;
static const int kTestPayloadType = 127;
static const uint32_t kTestSsrc = 1234;
static const uint32_t kTestTimestamp = 111111;
static const uint16_t kTestSeqNum = 4321;
static const int kRefFrameId = 17;

class RtpParserTest : public ::testing::Test {
 protected:
  RtpParserTest() : rtp_parser_(kTestSsrc, kTestPayloadType) {
    packet_builder_.SetSsrc(kTestSsrc);
    packet_builder_.SetSequenceNumber(kTestSeqNum);
    packet_builder_.SetTimestamp(kTestTimestamp);
    packet_builder_.SetPayloadType(kTestPayloadType);
    packet_builder_.SetMarkerBit(true);  // Only one packet.
    cast_header_.sender_ssrc = kTestSsrc;
    cast_header_.sequence_number = kTestSeqNum;
    cast_header_.rtp_timestamp = RtpTimeTicks().Expand(kTestTimestamp);
    cast_header_.payload_type = kTestPayloadType;
    cast_header_.marker = true;
  }

  ~RtpParserTest() override = default;

  void ExpectParsesPacket() {
    RtpCastHeader parsed_header;
    const uint8_t* payload = NULL;
    size_t payload_size = static_cast<size_t>(-1);
    EXPECT_TRUE(rtp_parser_.ParsePacket(
        packet_, kPacketLength, &parsed_header, &payload, &payload_size));

    EXPECT_EQ(cast_header_.marker, parsed_header.marker);
    EXPECT_EQ(cast_header_.payload_type, parsed_header.payload_type);
    EXPECT_EQ(cast_header_.sequence_number, parsed_header.sequence_number);
    EXPECT_EQ(cast_header_.rtp_timestamp, parsed_header.rtp_timestamp);
    EXPECT_EQ(cast_header_.sender_ssrc, parsed_header.sender_ssrc);

    EXPECT_EQ(cast_header_.is_key_frame, parsed_header.is_key_frame);
    EXPECT_EQ(cast_header_.frame_id, parsed_header.frame_id);
    EXPECT_EQ(cast_header_.packet_id, parsed_header.packet_id);
    EXPECT_EQ(cast_header_.max_packet_id, parsed_header.max_packet_id);
    EXPECT_EQ(cast_header_.reference_frame_id,
              parsed_header.reference_frame_id);

    EXPECT_TRUE(payload);
    EXPECT_NE(static_cast<size_t>(-1), payload_size);
  }

  void ExpectDoesNotParsePacket() {
    RtpCastHeader parsed_header;
    const uint8_t* payload = NULL;
    size_t payload_size = static_cast<size_t>(-1);
    EXPECT_FALSE(rtp_parser_.ParsePacket(
        packet_, kPacketLength, &parsed_header, &payload, &payload_size));
  }

  RtpPacketBuilder packet_builder_;
  uint8_t packet_[kPacketLength];
  RtpParser rtp_parser_;
  RtpCastHeader cast_header_;
};

TEST_F(RtpParserTest, ParseNonDefaultCastPacket) {
  packet_builder_.SetKeyFrame(true);
  packet_builder_.SetFrameIds(10, 10);
  packet_builder_.SetPacketId(5);
  packet_builder_.SetMaxPacketId(15);
  packet_builder_.SetMarkerBit(false);
  packet_builder_.BuildHeader(packet_, kPacketLength);
  cast_header_.is_key_frame = true;
  cast_header_.frame_id = FrameId::first() + 10;
  cast_header_.reference_frame_id = FrameId::first() + 10;
  cast_header_.packet_id = 5;
  cast_header_.max_packet_id = 15;
  cast_header_.marker = false;
  ExpectParsesPacket();
}

TEST_F(RtpParserTest, TooBigPacketId) {
  packet_builder_.SetKeyFrame(true);
  packet_builder_.SetFrameIds(10, 10);
  packet_builder_.SetPacketId(15);
  packet_builder_.SetMaxPacketId(5);
  packet_builder_.BuildHeader(packet_, kPacketLength);
  cast_header_.is_key_frame = true;
  cast_header_.frame_id = FrameId::first() + 10;
  cast_header_.reference_frame_id = FrameId::first() + 10;
  cast_header_.packet_id = 15;
  cast_header_.max_packet_id = 5;
  ExpectDoesNotParsePacket();
}

TEST_F(RtpParserTest, MaxPacketId) {
  packet_builder_.SetKeyFrame(true);
  packet_builder_.SetFrameIds(10, 10);
  packet_builder_.SetPacketId(65535);
  packet_builder_.SetMaxPacketId(65535);
  packet_builder_.BuildHeader(packet_, kPacketLength);
  cast_header_.is_key_frame = true;
  cast_header_.frame_id = FrameId::first() + 10;
  cast_header_.reference_frame_id = FrameId::first() + 10;
  cast_header_.packet_id = 65535;
  cast_header_.max_packet_id = 65535;
  ExpectParsesPacket();
}

TEST_F(RtpParserTest, InvalidPayloadType) {
  packet_builder_.SetKeyFrame(true);
  packet_builder_.SetFrameIds(10, 10);
  packet_builder_.SetPacketId(65535);
  packet_builder_.SetMaxPacketId(65535);
  packet_builder_.SetPayloadType(kTestPayloadType - 1);
  packet_builder_.BuildHeader(packet_, kPacketLength);
  cast_header_.is_key_frame = true;
  cast_header_.frame_id = FrameId::first() + 10;
  cast_header_.reference_frame_id = FrameId::first() + 10;
  cast_header_.packet_id = 65535;
  cast_header_.max_packet_id = 65535;
  cast_header_.payload_type = kTestPayloadType - 1;
  ExpectDoesNotParsePacket();
}

TEST_F(RtpParserTest, InvalidSsrc) {
  packet_builder_.SetKeyFrame(true);
  packet_builder_.SetFrameIds(10, 10);
  packet_builder_.SetPacketId(65535);
  packet_builder_.SetMaxPacketId(65535);
  packet_builder_.SetSsrc(kTestSsrc - 1);
  packet_builder_.BuildHeader(packet_, kPacketLength);
  cast_header_.is_key_frame = true;
  cast_header_.frame_id = FrameId::first() + 10;
  cast_header_.reference_frame_id = FrameId::first() + 10;
  cast_header_.packet_id = 65535;
  cast_header_.max_packet_id = 65535;
  cast_header_.sender_ssrc = kTestSsrc - 1;
  ExpectDoesNotParsePacket();
}

TEST_F(RtpParserTest, ParseCastPacketWithSpecificFrameReference) {
  packet_builder_.SetFrameIds(kRefFrameId + 3, kRefFrameId);
  packet_builder_.BuildHeader(packet_, kPacketLength);
  cast_header_.frame_id = FrameId::first() + kRefFrameId + 3;
  cast_header_.reference_frame_id = FrameId::first() + kRefFrameId;
  ExpectParsesPacket();
}

TEST_F(RtpParserTest, ParseExpandingFrameIdTo32Bits) {
  const int kMaxFrameId = 1000;
  packet_builder_.SetKeyFrame(true);
  cast_header_.is_key_frame = true;
  for (int frame_id = 0; frame_id <= kMaxFrameId; ++frame_id) {
    packet_builder_.SetFrameIds(frame_id, frame_id);
    packet_builder_.BuildHeader(packet_, kPacketLength);
    cast_header_.frame_id = FrameId::first() + frame_id;
    cast_header_.reference_frame_id = FrameId::first() + frame_id;
    ExpectParsesPacket();
  }
}

TEST_F(RtpParserTest, ParseExpandingReferenceFrameIdTo32Bits) {
  const int kMaxFrameId = 1000;
  const int kMaxBackReferenceOffset = 10;
  packet_builder_.SetKeyFrame(false);
  cast_header_.is_key_frame = false;
  for (int frame_id = kMaxBackReferenceOffset; frame_id <= kMaxFrameId;
       ++frame_id) {
    const int reference_frame_id =
        frame_id - base::RandInt(1, kMaxBackReferenceOffset);
    packet_builder_.SetFrameIds(frame_id, reference_frame_id);
    packet_builder_.BuildHeader(packet_, kPacketLength);
    cast_header_.frame_id = FrameId::first() + frame_id;
    cast_header_.reference_frame_id = FrameId::first() + reference_frame_id;
    ExpectParsesPacket();
  }
}

}  //  namespace cast
}  //  namespace media
