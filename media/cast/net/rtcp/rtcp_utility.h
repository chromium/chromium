// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_NET_RTCP_RTCP_UTILITY_H_
#define MEDIA_CAST_NET_RTCP_RTCP_UTILITY_H_

#include <stddef.h>
#include <stdint.h>

#include "base/big_endian.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/rtcp/rtcp_defines.h"

namespace media {
namespace cast {

// RFC 3550 page 44, including end null.
static const size_t kRtcpCnameSize = 256;

static const uint32_t kCast = ('C' << 24) + ('A' << 16) + ('S' << 8) + 'T';

static const uint32_t kCst2 = ('C' << 24) + ('S' << 16) + ('T' << 8) + '2';

static const uint8_t kReceiverLogSubtype = 2;

static const size_t kRtcpMaxReceiverLogMessages = 256;
static const size_t kRtcpMaxCastLossFields = 100;

struct RtcpCommonHeader {
  uint8_t V;   // Version.
  bool P;    // Padding.
  uint8_t IC;  // Item count / subtype.
  uint8_t PT;  // Packet Type.
  size_t length_in_octets;
};

class RtcpParser {
 public:
  RtcpParser(uint32_t local_ssrc, uint32_t remote_ssrc);

  RtcpParser(const RtcpParser&) = delete;
  RtcpParser& operator=(const RtcpParser&) = delete;

  ~RtcpParser();

  // Gets/Sets the ID of the latest frame that could possibly be ACK'ed.  This
  // is used when expanding truncated frame IDs during Parse().  This only needs
  // to be called if the client uses cast_message().
  FrameId max_valid_frame_id() const { return max_valid_frame_id_; }
  void SetMaxValidFrameId(FrameId max_valid_frame_id);

  // Parse the RTCP packet.
  bool Parse(base::BigEndianReader* reader);

  bool has_sender_report() const { return has_sender_report_; }
  const RtcpSenderInfo& sender_report() const {
    return sender_report_;
  }

  bool has_last_report() const { return has_last_report_; }
  uint32_t last_report() const { return last_report_; }
  uint32_t delay_since_last_report() const { return delay_since_last_report_; }

  bool has_receiver_log() const { return !receiver_log_.empty(); }
  const RtcpReceiverLogMessage& receiver_log() const { return receiver_log_; }
  RtcpReceiverLogMessage* mutable_receiver_log() { return & receiver_log_; }

  bool has_cast_message() const { return has_cast_message_; }
  const RtcpCastMessage& cast_message() const { return cast_message_; }
  RtcpCastMessage* mutable_cast_message() { return &cast_message_; }
  // Return if successfully parsed the extended feedback.
  bool has_cst2_message() const { return has_cst2_message_; }

  bool has_receiver_reference_time_report() const {
    return has_receiver_reference_time_report_;
  }
  const RtcpReceiverReferenceTimeReport&
  receiver_reference_time_report() const {
    return receiver_reference_time_report_;
  }

  bool has_picture_loss_indicator() const {
    return has_picture_loss_indicator_;
  }

 private:
  bool ParseCommonHeader(base::BigEndianReader* reader,
                         RtcpCommonHeader* parsed_header);
  bool ParseSR(base::BigEndianReader* reader, const RtcpCommonHeader& header);
  bool ParseRR(base::BigEndianReader* reader, const RtcpCommonHeader& header);
  bool ParseReportBlock(base::BigEndianReader* reader);
  bool ParsePli(base::BigEndianReader* reader, const RtcpCommonHeader& header);
  bool ParseApplicationDefined(base::BigEndianReader* reader,
                               const RtcpCommonHeader& header);
  bool ParseCastReceiverLogFrameItem(base::BigEndianReader* reader);
  bool ParseFeedbackCommon(base::BigEndianReader* reader,
                           const RtcpCommonHeader& header);
  bool ParseExtendedReport(base::BigEndianReader* reader,
                           const RtcpCommonHeader& header);
  bool ParseExtendedReportReceiverReferenceTimeReport(
      base::BigEndianReader* reader,
      uint32_t remote_ssrc);
  bool ParseExtendedReportDelaySinceLastReceiverReport(
      base::BigEndianReader* reader);

  const uint32_t local_ssrc_;
  const uint32_t remote_ssrc_;

  bool has_sender_report_;
  RtcpSenderInfo sender_report_;

  uint32_t last_report_;
  uint32_t delay_since_last_report_;
  bool has_last_report_;

  // |receiver_log_| is a vector vector, no need for has_*.
  RtcpReceiverLogMessage receiver_log_;

  bool has_cast_message_;
  RtcpCastMessage cast_message_;
  bool has_cst2_message_;

  bool has_receiver_reference_time_report_;
  RtcpReceiverReferenceTimeReport receiver_reference_time_report_;

  // Tracks recently-parsed RTP timestamps so that the truncated values can be
  // re-expanded into full-form.
  RtpTimeTicks last_parsed_sr_rtp_timestamp_;
  RtpTimeTicks last_parsed_frame_log_rtp_timestamp_;

  // The maximum possible re-expanded frame ID value.
  FrameId max_valid_frame_id_;

  // Indicates if sender received the Pli message from the receiver.
  bool has_picture_loss_indicator_;
};

// Converts a log event type to an integer value.
// NOTE: We have only allocated 4 bits to represent the type of event over the
// wire. Therefore, this function can only return values from 0 to 15.
uint8_t ConvertEventTypeToWireFormat(CastLoggingEvent event);

// The inverse of |ConvertEventTypeToWireFormat()|.
CastLoggingEvent TranslateToLogEventFromWireFormat(uint8_t event);

// Splits an NTP timestamp having a microsecond timebase into the standard two
// 32-bit integer wire format.
void ConvertTimeToFractions(int64_t ntp_time_us,
                            uint32_t* seconds,
                            uint32_t* fractions);

// Maps a base::TimeTicks value to an NTP timestamp comprised of two components.
void ConvertTimeTicksToNtp(const base::TimeTicks& time,
                           uint32_t* ntp_seconds,
                           uint32_t* ntp_fractions);

// Create a NTP diff from seconds and fractions of seconds; delay_fraction is
// fractions of a second where 0x80000000 is half a second.
uint32_t ConvertToNtpDiff(uint32_t delay_seconds, uint32_t delay_fraction);

// Maps an NTP timestamp, comprised of two components, to a base::TimeTicks
// value.
base::TimeTicks ConvertNtpToTimeTicks(uint32_t ntp_seconds,
                                      uint32_t ntp_fractions);

bool IsRtcpPacket(const uint8_t* packet, size_t length);

uint32_t GetSsrcOfSender(const uint8_t* rtcp_buffer, size_t length);

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_NET_RTCP_RTCP_UTILITY_H_
