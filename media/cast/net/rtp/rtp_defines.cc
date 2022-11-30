// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtp/rtp_defines.h"

namespace media {
namespace cast {

RtpCastHeader::RtpCastHeader()
    : marker(false),
      payload_type(0),
      sequence_number(0),
      sender_ssrc(0),
      is_key_frame(false),
      packet_id(0),
      max_packet_id(0),
      new_playout_delay_ms(0) {}

RtpPayloadFeedback::~RtpPayloadFeedback() = default;

}  // namespace cast
}  // namespace media
