// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_RTCP_RTCP_DEFINES_H_
#define MEDIA_CAST_NET_RTCP_RTCP_DEFINES_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/net/cast_transport_defines.h"

namespace media {
namespace cast {

static const size_t kRtcpCastLogHeaderSize = 12;
static const size_t kRtcpReceiverFrameLogSize = 8;
static const size_t kRtcpReceiverEventLogSize = 4;

// The maximum number of Cast receiver events to keep in history for the
// purpose of sending the events through RTCP.
// The number chosen should be more than the number of events that can be
// stored in a RTCP packet.
const size_t kReceiverRtcpEventHistorySize = 512;

enum RtcpPacketFields {
  kPacketTypeLow = 194,  // SMPTE time-code mapping.
  kPacketTypeSenderReport = 200,
  kPacketTypeReceiverReport = 201,
  kPacketTypeApplicationDefined = 204,
  kPacketTypeGenericRtpFeedback = 205,
  kPacketTypePayloadSpecific = 206,
  kPacketTypeXr = 207,
  kPacketTypeHigh = 210,  // Port Mapping.
};

// Handle the per frame ACK and NACK messages.
struct RtcpCastMessage {
  explicit RtcpCastMessage(uint32_t ssrc);
  RtcpCastMessage();
  ~RtcpCastMessage();

  uint32_t remote_ssrc;
  FrameId ack_frame_id;
  uint16_t target_delay_ms;
  MissingFramesAndPacketsMap missing_frames_and_packets;
  // This wrap-around counter is incremented by one for each ACK/NACK Cast
  // packet sent.
  uint8_t feedback_count;
  // The set of received frames that have frame IDs strictly equal to or larger
  // than |ack_frame_id + 2|.
  std::vector<FrameId> received_later_frames;
};

struct RtcpPliMessage {
  explicit RtcpPliMessage(uint32_t ssrc);
  RtcpPliMessage();

  uint32_t remote_ssrc;
};

// Log messages from receiver to sender.
struct RtcpReceiverEventLogMessage {
  RtcpReceiverEventLogMessage();
  ~RtcpReceiverEventLogMessage();

  CastLoggingEvent type;
  base::TimeTicks event_timestamp;
  base::TimeDelta delay_delta;
  uint16_t packet_id;
};

typedef std::list<RtcpReceiverEventLogMessage> RtcpReceiverEventLogMessages;

struct RtcpReceiverFrameLogMessage {
  explicit RtcpReceiverFrameLogMessage(RtpTimeTicks rtp_timestamp);
  RtcpReceiverFrameLogMessage(const RtcpReceiverFrameLogMessage& other);
  ~RtcpReceiverFrameLogMessage();

  const RtpTimeTicks rtp_timestamp_;
  RtcpReceiverEventLogMessages event_log_messages_;
};

typedef std::list<RtcpReceiverFrameLogMessage> RtcpReceiverLogMessage;

struct RtcpReceiverReferenceTimeReport {
  RtcpReceiverReferenceTimeReport();
  ~RtcpReceiverReferenceTimeReport();

  uint32_t remote_ssrc;
  uint32_t ntp_seconds;
  uint32_t ntp_fraction;
};

inline bool operator==(RtcpReceiverReferenceTimeReport lhs,
                       RtcpReceiverReferenceTimeReport rhs) {
  return lhs.remote_ssrc == rhs.remote_ssrc &&
         lhs.ntp_seconds == rhs.ntp_seconds &&
         lhs.ntp_fraction == rhs.ntp_fraction;
}

// Struct used by raw event subscribers as an intermediate format before
// sending off to the other side via RTCP.
// (i.e., {Sender,Receiver}RtcpEventSubscriber)
struct RtcpEvent {
  RtcpEvent();
  ~RtcpEvent();

  CastLoggingEvent type;

  // Time of event logged.
  base::TimeTicks timestamp;

  // Render/playout delay. Only set for FRAME_PLAYOUT events.
  base::TimeDelta delay_delta;

  // Only set for packet events.
  uint16_t packet_id;
};

// TODO(hubbe): Document members of this struct.
struct RtpReceiverStatistics {
  RtpReceiverStatistics();
  uint8_t fraction_lost;
  uint32_t cumulative_lost;  // 24 bits valid.
  uint32_t extended_high_sequence_number;
  uint32_t jitter;
};

// Created on a RTP receiver to be passed over IPC.
struct RtcpTimeData {
  uint32_t ntp_seconds;
  uint32_t ntp_fraction;
  base::TimeTicks timestamp;
};

// This struct is used to encapsulate all the parameters of the
// SendRtcpFromRtpReceiver for IPC transportation.
struct SendRtcpFromRtpReceiver_Params {
  SendRtcpFromRtpReceiver_Params();
  ~SendRtcpFromRtpReceiver_Params();
  uint32_t ssrc;
  uint32_t sender_ssrc;
  RtcpTimeData time_data;
  std::unique_ptr<RtcpCastMessage> cast_message;
  std::unique_ptr<RtcpPliMessage> pli_message;
  base::TimeDelta target_delay;
  std::unique_ptr<std::vector<std::pair<RtpTimeTicks, RtcpEvent>>> rtcp_events;
  std::unique_ptr<RtpReceiverStatistics> rtp_receiver_statistics;
};


}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_RTCP_RTCP_DEFINES_H_
