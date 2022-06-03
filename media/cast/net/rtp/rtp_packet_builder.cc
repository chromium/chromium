// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtp/rtp_packet_builder.h"

#include "base/big_endian.h"
#include "base/check_op.h"

namespace media {
namespace cast {

RtpPacketBuilder::RtpPacketBuilder()
    : is_key_(false),
      frame_id_(0),
      packet_id_(0),
      max_packet_id_(0),
      reference_frame_id_(0),
      timestamp_(0),
      sequence_number_(0),
      marker_(false),
      payload_type_(0),
      ssrc_(0) {}

void RtpPacketBuilder::SetKeyFrame(bool is_key) { is_key_ = is_key; }

void RtpPacketBuilder::SetFrameIds(uint32_t frame_id,
                                   uint32_t reference_frame_id) {
  frame_id_ = frame_id;
  reference_frame_id_ = reference_frame_id;
}

void RtpPacketBuilder::SetPacketId(uint16_t packet_id) {
  packet_id_ = packet_id;
}

void RtpPacketBuilder::SetMaxPacketId(uint16_t max_packet_id) {
  max_packet_id_ = max_packet_id;
}

void RtpPacketBuilder::SetTimestamp(uint32_t timestamp) {
  timestamp_ = timestamp;
}

void RtpPacketBuilder::SetSequenceNumber(uint16_t sequence_number) {
  sequence_number_ = sequence_number;
}

void RtpPacketBuilder::SetMarkerBit(bool marker) { marker_ = marker; }

void RtpPacketBuilder::SetPayloadType(int payload_type) {
  payload_type_ = payload_type;
}

void RtpPacketBuilder::SetSsrc(uint32_t ssrc) {
  ssrc_ = ssrc;
}

void RtpPacketBuilder::BuildHeader(uint8_t* data, uint32_t data_length) {
  BuildCommonHeader(data, data_length);
  BuildCastHeader(data + kRtpHeaderLength, data_length - kRtpHeaderLength);
}

void RtpPacketBuilder::BuildCastHeader(uint8_t* data, uint32_t data_length) {
  // Build header.
  DCHECK_LE(kCastHeaderLength, data_length);
  // Set the first 7 bytes to 0.
  memset(data, 0, kCastHeaderLength);
  base::BigEndianWriter big_endian_writer(reinterpret_cast<char*>(data), 56);
  const bool includes_specific_frame_reference =
      (is_key_ && (reference_frame_id_ != frame_id_)) ||
      (!is_key_ && (reference_frame_id_ != (frame_id_ - 1)));
  big_endian_writer.WriteU8((is_key_ ? 0x80 : 0) |
                            (includes_specific_frame_reference ? 0x40 : 0));
  big_endian_writer.WriteU8(frame_id_);
  big_endian_writer.WriteU16(packet_id_);
  big_endian_writer.WriteU16(max_packet_id_);
  if (includes_specific_frame_reference) {
    big_endian_writer.WriteU8(reference_frame_id_);
  }
}

void RtpPacketBuilder::BuildCommonHeader(uint8_t* data, uint32_t data_length) {
  DCHECK_LE(kRtpHeaderLength, data_length);
  base::BigEndianWriter big_endian_writer(reinterpret_cast<char*>(data), 96);
  big_endian_writer.WriteU8(0x80);
  big_endian_writer.WriteU8(payload_type_ | (marker_ ? kRtpMarkerBitMask : 0));
  big_endian_writer.WriteU16(sequence_number_);
  big_endian_writer.WriteU32(timestamp_);
  big_endian_writer.WriteU32(ssrc_);
}

}  // namespace cast
}  // namespace media
