// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_RTP_RTP_DEFINES_H_
#define MEDIA_CAST_NET_RTP_RTP_DEFINES_H_

#include <stdint.h>

#include "media/cast/net/rtcp/rtcp_defines.h"

namespace media {
namespace cast {

static const uint16_t kRtpHeaderLength = 12;
static const uint16_t kCastHeaderLength = 7;

// RTP Header
static const uint8_t kRtpExtensionBitMask = 0x10;
static const uint8_t kRtpMarkerBitMask = 0x80;
static const uint8_t kRtpNumCsrcsMask = 0x0f;

// Cast Header
static const uint8_t kCastKeyFrameBitMask = 0x80;
static const uint8_t kCastReferenceFrameIdBitMask = 0x40;
static const uint8_t kCastExtensionCountmask = 0x3f;

// Cast RTP extensions.
static const uint8_t kCastRtpExtensionAdaptiveLatency = 1;

struct RtpCastHeader {
  RtpCastHeader();
  // Elements from RTP packet header.
  bool marker;
  uint8_t payload_type;
  uint16_t sequence_number;
  RtpTimeTicks rtp_timestamp;
  uint32_t sender_ssrc;
  uint8_t num_csrcs;

  // Elements from Cast header (at beginning of RTP payload).
  bool is_key_frame;
  bool is_reference;
  FrameId frame_id;
  uint16_t packet_id;
  uint16_t max_packet_id;
  FrameId reference_frame_id;
  uint16_t new_playout_delay_ms;
  uint8_t num_extensions;
};

class RtpPayloadFeedback {
 public:
  virtual void CastFeedback(const RtcpCastMessage& cast_feedback) = 0;

 protected:
  virtual ~RtpPayloadFeedback();
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_RTP_RTP_DEFINES_H_
