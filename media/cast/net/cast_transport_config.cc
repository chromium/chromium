// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/cast_transport_config.h"

namespace media {
namespace cast {

CastTransportRtpConfig::CastTransportRtpConfig()
    : rtp_stream_id(0),
      ssrc(0),
      feedback_ssrc(0),
      rtp_payload_type(RtpPayloadType::UNKNOWN) {}

CastTransportRtpConfig::~CastTransportRtpConfig() = default;

RtcpSenderInfo::RtcpSenderInfo()
    : ntp_seconds(0),
      ntp_fraction(0),
      send_packet_count(0),
      send_octet_count(0) {}
RtcpSenderInfo::~RtcpSenderInfo() = default;

RtcpReportBlock::RtcpReportBlock()
    : remote_ssrc(0),
      media_ssrc(0),
      fraction_lost(0),
      cumulative_lost(0),
      extended_high_sequence_number(0),
      jitter(0),
      last_sr(0),
      delay_since_last_sr(0) {}
RtcpReportBlock::~RtcpReportBlock() = default;

RtcpDlrrReportBlock::RtcpDlrrReportBlock()
    : last_rr(0), delay_since_last_rr(0) {}
RtcpDlrrReportBlock::~RtcpDlrrReportBlock() = default;

}  // namespace cast
}  // namespace media
