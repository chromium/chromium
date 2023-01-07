// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtcp/rtcp_defines.h"

#include "media/cast/logging/logging_defines.h"

namespace media {
namespace cast {

RtcpCastMessage::RtcpCastMessage(uint32_t ssrc)
    : remote_ssrc(ssrc), target_delay_ms(0) {}
RtcpCastMessage::RtcpCastMessage() : RtcpCastMessage(0) {}
RtcpCastMessage::~RtcpCastMessage() = default;

RtcpPliMessage::RtcpPliMessage(uint32_t ssrc) : remote_ssrc(ssrc) {}
RtcpPliMessage::RtcpPliMessage() : remote_ssrc(0) {}

RtcpReceiverEventLogMessage::RtcpReceiverEventLogMessage()
    : type(UNKNOWN), packet_id(0u) {}
RtcpReceiverEventLogMessage::~RtcpReceiverEventLogMessage() = default;

RtcpReceiverFrameLogMessage::RtcpReceiverFrameLogMessage(RtpTimeTicks timestamp)
    : rtp_timestamp_(timestamp) {}
RtcpReceiverFrameLogMessage::RtcpReceiverFrameLogMessage(
    const RtcpReceiverFrameLogMessage& other) = default;
RtcpReceiverFrameLogMessage::~RtcpReceiverFrameLogMessage() = default;

RtcpReceiverReferenceTimeReport::RtcpReceiverReferenceTimeReport()
    : remote_ssrc(0u), ntp_seconds(0u), ntp_fraction(0u) {}
RtcpReceiverReferenceTimeReport::~RtcpReceiverReferenceTimeReport() = default;

RtcpEvent::RtcpEvent() : type(UNKNOWN), packet_id(0u) {}
RtcpEvent::~RtcpEvent() = default;

RtpReceiverStatistics::RtpReceiverStatistics() :
    fraction_lost(0),
    cumulative_lost(0),
    extended_high_sequence_number(0),
    jitter(0) {}

SendRtcpFromRtpReceiver_Params::SendRtcpFromRtpReceiver_Params()
    : ssrc(0),
      sender_ssrc(0) {}

SendRtcpFromRtpReceiver_Params::~SendRtcpFromRtpReceiver_Params() = default;

}  // namespace cast
}  // namespace media
