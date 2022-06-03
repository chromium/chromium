// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtp/rtp_parser.h"

#include "base/big_endian.h"
#include "base/check.h"
#include "media/cast/constants.h"
#include "media/cast/net/rtp/rtp_defines.h"

namespace media {
namespace cast {

// static
bool RtpParser::ParseSsrc(const uint8_t* packet,
                          size_t length,
                          uint32_t* ssrc) {
  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(packet), length);
  return big_endian_reader.Skip(8) && big_endian_reader.ReadU32(ssrc);
}

RtpParser::RtpParser(uint32_t expected_sender_ssrc,
                     uint8_t expected_payload_type)
    : expected_sender_ssrc_(expected_sender_ssrc),
      expected_payload_type_(expected_payload_type),
      last_parsed_frame_id_(FrameId::first() - 1) {}

RtpParser::~RtpParser() = default;

bool RtpParser::ParsePacket(const uint8_t* packet,
                            size_t length,
                            RtpCastHeader* header,
                            const uint8_t** payload_data,
                            size_t* payload_size) {
  DCHECK(packet);
  DCHECK(header);
  DCHECK(payload_data);
  DCHECK(payload_size);

  if (length < (kRtpHeaderLength + kCastHeaderLength))
    return false;

  base::BigEndianReader reader(reinterpret_cast<const char*>(packet), length);

  // Parse the RTP header.  See
  // http://en.wikipedia.org/wiki/Real-time_Transport_Protocol for an
  // explanation of the standard RTP packet header.
  uint8_t bits;
  if (!reader.ReadU8(&bits))
    return false;
  const uint8_t version = bits >> 6;
  if (version != 2)
    return false;
  header->num_csrcs = bits & kRtpNumCsrcsMask;
  if (bits & kRtpExtensionBitMask)
    return false;  // We lack the implementation to skip over an extension.
  if (!reader.ReadU8(&bits))
    return false;
  header->marker = !!(bits & kRtpMarkerBitMask);
  header->payload_type = bits & ~kRtpMarkerBitMask;
  if (header->payload_type != expected_payload_type_)
    return false;  // Punt: Unexpected payload type.
  uint32_t truncated_rtp_timestamp;
  if (!reader.ReadU16(&header->sequence_number) ||
      !reader.ReadU32(&truncated_rtp_timestamp) ||
      !reader.ReadU32(&header->sender_ssrc) ||
      header->sender_ssrc != expected_sender_ssrc_) {
    return false;
  }
  header->rtp_timestamp =
      last_parsed_rtp_timestamp_.Expand(truncated_rtp_timestamp);

  // Parse the Cast header.  Note that, from the RTP protocol's perspective, the
  // Cast header is part of the payload (and not meant to be an extension
  // header).
  if (!reader.ReadU8(&bits))
    return false;
  header->is_key_frame = !!(bits & kCastKeyFrameBitMask);
  header->is_reference = !!(bits & kCastReferenceFrameIdBitMask);
  uint8_t truncated_frame_id;
  if (!reader.ReadU8(&truncated_frame_id) ||
      !reader.ReadU16(&header->packet_id) ||
      !reader.ReadU16(&header->max_packet_id)) {
    return false;
  }
  // Sanity-check: Do the packet ID values make sense w.r.t. each other?
  if (header->max_packet_id < header->packet_id)
    return false;
  uint8_t truncated_reference_frame_id;
  if (!header->is_reference) {
    // By default, a key frame only references itself; and non-key frames
    // reference their direct predecessor.
    truncated_reference_frame_id = truncated_frame_id;
    if (!header->is_key_frame)
      --truncated_reference_frame_id;
  } else if (!reader.ReadU8(&truncated_reference_frame_id)) {
    return false;
  }

  header->num_extensions = bits & kCastExtensionCountmask;
  for (int i = 0; i < header->num_extensions; i++) {
    uint16_t type_and_size;
    if (!reader.ReadU16(&type_and_size))
      return false;
    base::StringPiece tmp;
    if (!reader.ReadPiece(&tmp, type_and_size & 0x3ff))
      return false;
    base::BigEndianReader chunk(tmp.data(), tmp.size());
    switch (type_and_size >> 10) {
      case kCastRtpExtensionAdaptiveLatency:
        if (!chunk.ReadU16(&header->new_playout_delay_ms))
          return false;
    }
  }

  last_parsed_rtp_timestamp_ = header->rtp_timestamp;

  header->frame_id = last_parsed_frame_id_.Expand(truncated_frame_id);
  header->reference_frame_id =
      header->frame_id.Expand(truncated_reference_frame_id);
  last_parsed_frame_id_ = header->frame_id;

  // All remaining data in the packet is the payload.
  *payload_data = reinterpret_cast<const uint8_t*>(reader.ptr());
  *payload_size = reader.remaining();

  return true;
}

}  // namespace cast
}  // namespace media
