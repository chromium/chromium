// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/test_rtcp_packet_builder.h"

#include <memory>

#include "base/check_op.h"
#include "media/cast/net/rtcp/rtcp_utility.h"

namespace media {
namespace cast {

TestRtcpPacketBuilder::TestRtcpPacketBuilder()
    : writer_(buffer_), big_endian_reader_(nullptr, 0) {}

TestRtcpPacketBuilder::~TestRtcpPacketBuilder() = default;

void TestRtcpPacketBuilder::AddSr(uint32_t remote_ssrc,
                                  int number_of_report_blocks) {
  AddRtcpHeader(200, number_of_report_blocks);
  writer_.WriteU32BigEndian(remote_ssrc);
  writer_.WriteU32BigEndian(kNtpHigh);  // NTP timestamp.
  writer_.WriteU32BigEndian(kNtpLow);
  writer_.WriteU32BigEndian(kRtpTimestamp);
  writer_.WriteU32BigEndian(kSendPacketCount);
  writer_.WriteU32BigEndian(kSendOctetCount);
}

void TestRtcpPacketBuilder::AddSrWithNtp(uint32_t remote_ssrc,
                                         uint32_t ntp_high,
                                         uint32_t ntp_low,
                                         uint32_t rtp_timestamp) {
  AddRtcpHeader(200, 0);
  writer_.WriteU32BigEndian(remote_ssrc);
  writer_.WriteU32BigEndian(ntp_high);
  writer_.WriteU32BigEndian(ntp_low);
  writer_.WriteU32BigEndian(rtp_timestamp);
  writer_.WriteU32BigEndian(kSendPacketCount);
  writer_.WriteU32BigEndian(kSendOctetCount);
}

void TestRtcpPacketBuilder::AddRr(uint32_t remote_ssrc,
                                  int number_of_report_blocks) {
  AddRtcpHeader(201, number_of_report_blocks);
  writer_.WriteU32BigEndian(remote_ssrc);
}

void TestRtcpPacketBuilder::AddRb(uint32_t local_ssrc) {
  writer_.WriteU32BigEndian(local_ssrc);
  writer_.WriteU32BigEndian(kLoss);
  writer_.WriteU32BigEndian(kExtendedMax);
  writer_.WriteU32BigEndian(kTestJitter);
  writer_.WriteU32BigEndian(kLastSr);
  writer_.WriteU32BigEndian(kDelayLastSr);
}

void TestRtcpPacketBuilder::AddXrHeader(uint32_t remote_ssrc) {
  AddRtcpHeader(207, 0);
  writer_.WriteU32BigEndian(remote_ssrc);
}

void TestRtcpPacketBuilder::AddXrUnknownBlock() {
  writer_.WriteU8BigEndian(9);   // Block type.
  writer_.WriteU8BigEndian(0);   // Reserved.
  writer_.WriteU16BigEndian(4);  // Block length.
  // First receiver same as sender of this report.
  writer_.WriteU32BigEndian(0);
  writer_.WriteU32BigEndian(0);
  writer_.WriteU32BigEndian(0);
  writer_.WriteU32BigEndian(0);
}

void TestRtcpPacketBuilder::AddUnknownBlock() {
  AddRtcpHeader(99, 0);
  writer_.WriteU32BigEndian(42);
  writer_.WriteU32BigEndian(42);
  writer_.WriteU32BigEndian(42);
}

void TestRtcpPacketBuilder::AddXrDlrrBlock(uint32_t remote_ssrc) {
  writer_.WriteU8BigEndian(5);   // Block type.
  writer_.WriteU8BigEndian(0);   // Reserved.
  writer_.WriteU16BigEndian(3);  // Block length.

  // First receiver same as sender of this report.
  writer_.WriteU32BigEndian(remote_ssrc);
  writer_.WriteU32BigEndian(kLastRr);
  writer_.WriteU32BigEndian(kDelayLastRr);
}

void TestRtcpPacketBuilder::AddXrExtendedDlrrBlock(uint32_t remote_ssrc) {
  writer_.WriteU8BigEndian(5);   // Block type.
  writer_.WriteU8BigEndian(0);   // Reserved.
  writer_.WriteU16BigEndian(9);  // Block length.
  writer_.WriteU32BigEndian(0xaaaaaaaa);
  writer_.WriteU32BigEndian(0xaaaaaaaa);
  writer_.WriteU32BigEndian(0xaaaaaaaa);

  // First receiver same as sender of this report.
  writer_.WriteU32BigEndian(remote_ssrc);
  writer_.WriteU32BigEndian(kLastRr);
  writer_.WriteU32BigEndian(kDelayLastRr);
  writer_.WriteU32BigEndian(0xbbbbbbbb);
  writer_.WriteU32BigEndian(0xbbbbbbbb);
  writer_.WriteU32BigEndian(0xbbbbbbbb);
}

void TestRtcpPacketBuilder::AddXrRrtrBlock() {
  writer_.WriteU8BigEndian(4);   // Block type.
  writer_.WriteU8BigEndian(0);   // Reserved.
  writer_.WriteU16BigEndian(2);  // Block length.
  writer_.WriteU32BigEndian(kNtpHigh);
  writer_.WriteU32BigEndian(kNtpLow);
}

void TestRtcpPacketBuilder::AddNack(uint32_t remote_ssrc, uint32_t local_ssrc) {
  AddRtcpHeader(205, 1);
  writer_.WriteU32BigEndian(remote_ssrc);
  writer_.WriteU32BigEndian(local_ssrc);
  writer_.WriteU16BigEndian(kMissingPacket);
  writer_.WriteU16BigEndian(0);
}

void TestRtcpPacketBuilder::AddSendReportRequest(uint32_t remote_ssrc,
                                                 uint32_t local_ssrc) {
  AddRtcpHeader(205, 5);
  writer_.WriteU32BigEndian(remote_ssrc);
  writer_.WriteU32BigEndian(local_ssrc);
}

void TestRtcpPacketBuilder::AddCast(uint32_t remote_ssrc,
                                    uint32_t local_ssrc,
                                    base::TimeDelta target_delay) {
  AddRtcpHeader(206, 15);
  writer_.WriteU32BigEndian(remote_ssrc);
  writer_.WriteU32BigEndian(local_ssrc);
  writer_.WriteU8BigEndian('C');
  writer_.WriteU8BigEndian('A');
  writer_.WriteU8BigEndian('S');
  writer_.WriteU8BigEndian('T');
  writer_.WriteU8BigEndian(kAckFrameId);
  writer_.WriteU8BigEndian(3);  // Loss fields.
  writer_.WriteU16BigEndian(target_delay.InMilliseconds());
  writer_.WriteU8BigEndian(kLostFrameId);
  writer_.WriteU16BigEndian(kRtcpCastAllPacketsLost);
  writer_.WriteU8BigEndian(0);  // Lost packet id mask.
  writer_.WriteU8BigEndian(kFrameIdWithLostPackets);
  writer_.WriteU16BigEndian(kLostPacketId1);
  writer_.WriteU8BigEndian(0x2);  // Lost packet id mask.
  writer_.WriteU8BigEndian(kFrameIdWithLostPackets);
  writer_.WriteU16BigEndian(kLostPacketId3);
  writer_.WriteU8BigEndian(0);  // Lost packet id mask.
}

void TestRtcpPacketBuilder::AddCst2(
    const std::vector<FrameId>& later_received_frames) {
  writer_.WriteU8BigEndian('C');
  writer_.WriteU8BigEndian('S');
  writer_.WriteU8BigEndian('T');
  writer_.WriteU8BigEndian('2');
  writer_.WriteU8BigEndian(kFeedbackSeq);

  std::vector<uint8_t> ack_bitmasks;
  for (FrameId ack_frame : later_received_frames) {
    const int64_t bit_index = ack_frame - (FrameId::first() + kAckFrameId) - 2;
    CHECK_LE(INT64_C(0), bit_index);
    const size_t index = static_cast<size_t>(bit_index) / 8;
    const size_t bit_index_within_byte = static_cast<size_t>(bit_index) % 8;
    if (index >= ack_bitmasks.size())
      ack_bitmasks.resize(index + 1);
    ack_bitmasks[index] |= 1 << bit_index_within_byte;
  }

  CHECK_LT(ack_bitmasks.size(), 256u);
  writer_.WriteU8BigEndian(ack_bitmasks.size());
  for (uint8_t ack_bits : ack_bitmasks)
    writer_.WriteU8BigEndian(ack_bits);

  // Pad to ensure the extra CST2 data chunk is 32-bit aligned.
  for (size_t num_bytes_written = 6 + ack_bitmasks.size();
       num_bytes_written % 4; ++num_bytes_written) {
    writer_.WriteU8BigEndian(0);
  }
}

void TestRtcpPacketBuilder::AddErrorCst2() {
  writer_.WriteU8BigEndian('C');
  writer_.WriteU8BigEndian('A');
  writer_.WriteU8BigEndian('S');
  writer_.WriteU8BigEndian('T');
  writer_.WriteU8BigEndian(kFeedbackSeq);
  writer_.WriteU8BigEndian(0);
  writer_.WriteU8BigEndian(0);
  writer_.WriteU8BigEndian(0);
}

void TestRtcpPacketBuilder::AddPli(uint32_t remote_ssrc, uint32_t local_ssrc) {
  AddRtcpHeader(206, 1);
  writer_.WriteU32BigEndian(remote_ssrc);
  writer_.WriteU32BigEndian(local_ssrc);
}

void TestRtcpPacketBuilder::AddReceiverLog(uint32_t remote_ssrc) {
  AddRtcpHeader(204, 2);
  writer_.WriteU32BigEndian(remote_ssrc);
  writer_.WriteU8BigEndian('C');
  writer_.WriteU8BigEndian('A');
  writer_.WriteU8BigEndian('S');
  writer_.WriteU8BigEndian('T');
}

void TestRtcpPacketBuilder::AddReceiverFrameLog(uint32_t rtp_timestamp,
                                                int num_events,
                                                uint32_t event_timesamp_base) {
  writer_.WriteU32BigEndian(rtp_timestamp);
  writer_.WriteU8BigEndian(static_cast<uint8_t>(num_events - 1));
  writer_.WriteU8BigEndian(static_cast<uint8_t>(event_timesamp_base >> 16));
  writer_.WriteU8BigEndian(static_cast<uint8_t>(event_timesamp_base >> 8));
  writer_.WriteU8BigEndian(static_cast<uint8_t>(event_timesamp_base));
}

void TestRtcpPacketBuilder::AddReceiverEventLog(uint16_t event_data,
                                                CastLoggingEvent event,
                                                uint16_t event_timesamp_delta) {
  writer_.WriteU16BigEndian(event_data);
  uint8_t event_id = ConvertEventTypeToWireFormat(event);
  uint16_t type_and_delta = static_cast<uint16_t>(event_id) << 12;
  type_and_delta += event_timesamp_delta & 0x0fff;
  writer_.WriteU16BigEndian(type_and_delta);
}

std::unique_ptr<media::cast::Packet> TestRtcpPacketBuilder::GetPacket() {
  PatchLengthField();
  return std::make_unique<media::cast::Packet>(buffer_.begin(),
                                               buffer_.begin() + Length());
}

const uint8_t* TestRtcpPacketBuilder::Data() {
  PatchLengthField();
  return buffer_.data();
}

base::BigEndianReader* TestRtcpPacketBuilder::Reader() {
  big_endian_reader_ = base::BigEndianReader(Data(), Length());
  return &big_endian_reader_;
}

void TestRtcpPacketBuilder::PatchLengthField() {
  if (pos_of_length_.has_value()) {
    // Back-patch the packet length. The client must have taken
    // care of proper padding to 32-bit words.
    size_t length = writer_.num_written() - *pos_of_length_ - 2u;
    DCHECK_EQ(0u, length % 4u) << "Packets must be a multiple of 32 bits long";
    base::span(buffer_)
        .subspan(*pos_of_length_)
        .first<2u>()
        .copy_from(base::U16ToBigEndian(length / 4u));
    pos_of_length_ = std::nullopt;
  }
}

// Set the 5-bit value in the 1st byte of the header
// and the payload type. Set aside room for the length field,
// and make provision for back-patching it.
void TestRtcpPacketBuilder::AddRtcpHeader(int payload, int format_or_count) {
  PatchLengthField();
  writer_.WriteU8BigEndian(0x80 | (format_or_count & 0x1F));
  writer_.WriteU8BigEndian(payload);

  // Save the position where the length will be written later.
  pos_of_length_ = writer_.num_written();
  // Initialize length to "clearly illegal".
  writer_.WriteU16BigEndian(0xDEAD);
}

}  // namespace cast
}  // namespace media
