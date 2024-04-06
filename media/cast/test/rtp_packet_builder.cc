// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/rtp_packet_builder.h"

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"

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

void RtpPacketBuilder::SetKeyFrame(bool is_key) {
  is_key_ = is_key;
}

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

void RtpPacketBuilder::SetMarkerBit(bool marker) {
  marker_ = marker;
}

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

void RtpPacketBuilder::BuildCastHeader(uint8_t* data_ptr,
                                       uint32_t data_length) {
  // TODO(crbug.com/40284755): This function should receive a span, not a
  // pointer.
  auto data = UNSAFE_BUFFERS(base::span(data_ptr, data_length));
  auto writer = base::SpanWriter(data.first<kCastHeaderLength>());
  const bool includes_specific_frame_reference =
      (is_key_ && (reference_frame_id_ != frame_id_)) ||
      (!is_key_ && (reference_frame_id_ != (frame_id_ - 1)));
  writer.WriteU8BigEndian((is_key_ ? 0x80 : 0) |
                          (includes_specific_frame_reference ? 0x40 : 0));
  writer.WriteU8BigEndian(frame_id_);
  writer.WriteU16BigEndian(packet_id_);
  writer.WriteU16BigEndian(max_packet_id_);
  if (includes_specific_frame_reference) {
    writer.WriteU8BigEndian(reference_frame_id_);
  } else {
    writer.WriteU8BigEndian(0u);
  }
  CHECK_EQ(writer.remaining(), 0u);
}

void RtpPacketBuilder::BuildCommonHeader(uint8_t* data_ptr,
                                         uint32_t data_length) {
  // TODO(crbug.com/40284755): This function should receive a span, not a
  // pointer.
  auto data = UNSAFE_BUFFERS(base::span(data_ptr, data_length));
  auto writer = base::SpanWriter(data.first<12u>());
  writer.WriteU8BigEndian(0x80);
  writer.WriteU8BigEndian(payload_type_ | (marker_ ? kRtpMarkerBitMask : 0));
  writer.WriteU16BigEndian(sequence_number_);
  writer.WriteU32BigEndian(timestamp_);
  writer.WriteU32BigEndian(ssrc_);
}

}  // namespace cast
}  // namespace media
