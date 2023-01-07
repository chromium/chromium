// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_RTP_RTP_PARSER_H_
#define MEDIA_CAST_NET_RTP_RTP_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/rtp/rtp_defines.h"

namespace media {
namespace cast {

class RtpParser {
 public:
  RtpParser(uint32_t expected_sender_ssrc, uint8_t expected_payload_type);

  RtpParser(const RtpParser&) = delete;
  RtpParser& operator=(const RtpParser&) = delete;

  virtual ~RtpParser();

  // Parses the |packet|, expecting an RTP header along with a Cast header at
  // the beginning of the the RTP payload.  This method populates the structure
  // pointed to by |rtp_header| and sets the |payload_data| pointer and
  // |payload_size| to the memory region within |packet| containing the Cast
  // payload data.  Returns false if the data appears to be invalid, is not from
  // the expected sender (as identified by the SSRC field), or is not the
  // expected payload type.
  bool ParsePacket(const uint8_t* packet,
                   size_t length,
                   RtpCastHeader* rtp_header,
                   const uint8_t** payload_data,
                   size_t* payload_size);

  static bool ParseSsrc(const uint8_t* packet, size_t length, uint32_t* ssrc);

 private:
  const uint32_t expected_sender_ssrc_;
  const uint8_t expected_payload_type_;

  // Tracks recently-parsed values so that the truncated values can be
  // re-expanded into full-form.
  RtpTimeTicks last_parsed_rtp_timestamp_;
  FrameId last_parsed_frame_id_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_RTP_RTP_PARSER_H_
