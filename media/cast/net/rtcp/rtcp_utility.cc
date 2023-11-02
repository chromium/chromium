// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtcp/rtcp_utility.h"

#include <stdint.h>

#include <cmath>

#include "base/logging.h"
#include "media/cast/net/cast_transport_defines.h"

namespace media {
namespace cast {

namespace {

// January 1970, in NTP seconds.
// Network Time Protocol (NTP), which is in seconds relative to 0h UTC on
// 1 January 1900.
const int64_t kUnixEpochInNtpSeconds = INT64_C(2208988800);

// Magic fractional unit. Used to convert time (in microseconds) to/from
// fractional NTP seconds.
const double kMagicFractionalUnit = 4.294967296E3;

}  // namespace

RtcpParser::RtcpParser(uint32_t local_ssrc, uint32_t remote_ssrc)
    : local_ssrc_(local_ssrc),
      remote_ssrc_(remote_ssrc),
      has_sender_report_(false),
      has_last_report_(false),
      has_cast_message_(false),
      has_cst2_message_(false),
      has_receiver_reference_time_report_(false),
      has_picture_loss_indicator_(false) {}

RtcpParser::~RtcpParser() = default;

void RtcpParser::SetMaxValidFrameId(FrameId frame_id) {
  max_valid_frame_id_ = frame_id;
}

bool RtcpParser::Parse(base::BigEndianReader* reader) {
  // Reset.
  has_sender_report_ = false;
  sender_report_ = RtcpSenderInfo();
  has_last_report_ = false;
  receiver_log_.clear();
  has_cast_message_ = false;
  has_cst2_message_ = false;
  has_receiver_reference_time_report_ = false;
  has_picture_loss_indicator_ = false;

  while (reader->remaining()) {
    RtcpCommonHeader header;
    if (!ParseCommonHeader(reader, &header))
      return false;

    base::span<const uint8_t> tmp;
    if (!reader->ReadSpan(&tmp, header.length_in_octets - 4))
      return false;
    base::BigEndianReader chunk(tmp);

    switch (header.PT) {
      case kPacketTypeSenderReport:
        if (!ParseSR(&chunk, header))
          return false;
        break;

      case kPacketTypeReceiverReport:
        if (!ParseRR(&chunk, header))
          return false;
        break;

      case kPacketTypeApplicationDefined:
        if (!ParseApplicationDefined(&chunk, header))
          return false;
        break;

      case kPacketTypePayloadSpecific: {
        if (!ParseFeedbackCommon(&chunk, header))
          return false;
        if (!ParsePli(&chunk, header))
          return false;
        break;
      }

      case kPacketTypeXr:
        if (!ParseExtendedReport(&chunk, header))
          return false;
        break;
    }
  }
  return true;
}

bool RtcpParser::ParseCommonHeader(base::BigEndianReader* reader,
                                   RtcpCommonHeader* parsed_header) {
  //  0                   1                   2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |V=2|P|    IC   |      PT       |             length            |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //
  // Common header for all Rtcp packets, 4 octets.

  uint8_t byte;
  if (!reader->ReadU8(&byte))
    return false;
  parsed_header->V = byte >> 6;
  parsed_header->P = ((byte & 0x20) == 0) ? false : true;

  // Check if RTP version field == 2.
  if (parsed_header->V != 2)
    return false;

  parsed_header->IC = byte & 0x1f;
  if (!reader->ReadU8(&parsed_header->PT))
    return false;

  uint16_t bytes;
  if (!reader->ReadU16(&bytes))
    return false;

  parsed_header->length_in_octets = (static_cast<size_t>(bytes) + 1) * 4;

  if (parsed_header->length_in_octets == 0)
    return false;

  return true;
}

bool RtcpParser::ParseSR(base::BigEndianReader* reader,
                         const RtcpCommonHeader& header) {
  uint32_t sender_ssrc;
  if (!reader->ReadU32(&sender_ssrc))
    return false;

  if (sender_ssrc != remote_ssrc_)
    return true;

  uint32_t truncated_rtp_timestamp;
  uint32_t send_octet_count;
  if (!reader->ReadU32(&sender_report_.ntp_seconds) ||
      !reader->ReadU32(&sender_report_.ntp_fraction) ||
      !reader->ReadU32(&truncated_rtp_timestamp) ||
      !reader->ReadU32(&sender_report_.send_packet_count) ||
      !reader->ReadU32(&send_octet_count))
    return false;
  sender_report_.rtp_timestamp = last_parsed_sr_rtp_timestamp_ =
      last_parsed_sr_rtp_timestamp_.Expand(truncated_rtp_timestamp);
  sender_report_.send_octet_count = send_octet_count;
  has_sender_report_ = true;

  for (size_t block = 0; block < header.IC; block++)
    if (!ParseReportBlock(reader))
      return false;

  return true;
}

bool RtcpParser::ParseRR(base::BigEndianReader* reader,
                         const RtcpCommonHeader& header) {
  uint32_t receiver_ssrc;
  if (!reader->ReadU32(&receiver_ssrc))
    return false;

  if (receiver_ssrc != remote_ssrc_)
    return true;

  for (size_t block = 0; block < header.IC; block++)
    if (!ParseReportBlock(reader))
      return false;

  return true;
}

bool RtcpParser::ParseReportBlock(base::BigEndianReader* reader) {
  uint32_t ssrc, last_report, delay;
  if (!reader->ReadU32(&ssrc) ||
      !reader->Skip(12) ||
      !reader->ReadU32(&last_report) ||
      !reader->ReadU32(&delay))
    return false;

  if (ssrc == local_ssrc_) {
    last_report_ = last_report;
    delay_since_last_report_ = delay;
    has_last_report_ = true;
  }

  return true;
}

bool RtcpParser::ParsePli(base::BigEndianReader* reader,
                          const RtcpCommonHeader& header) {
  if (header.IC != 1)
    return true;

  uint32_t receiver_ssrc, sender_ssrc;
  if (!reader->ReadU32(&receiver_ssrc))
    return false;

  // Ignore this Rtcp if the receiver ssrc does not match.
  if (receiver_ssrc != remote_ssrc_)
    return true;

  if (!reader->ReadU32(&sender_ssrc))
    return false;

  // Ignore this Rtcp if the sender ssrc does not match.
  if (sender_ssrc != local_ssrc_)
    return true;

  has_picture_loss_indicator_ = true;
  return true;
}

bool RtcpParser::ParseApplicationDefined(base::BigEndianReader* reader,
                                         const RtcpCommonHeader& header) {
  uint32_t sender_ssrc;
  uint32_t name;
  if (!reader->ReadU32(&sender_ssrc) ||
      !reader->ReadU32(&name))
    return false;

  if (sender_ssrc != remote_ssrc_)
    return true;

  if (name != kCast)
    return false;

  switch (header.IC) {  // subtype
    case kReceiverLogSubtype:
      if (!ParseCastReceiverLogFrameItem(reader))
        return false;
      break;
  }
  return true;
}

bool RtcpParser::ParseCastReceiverLogFrameItem(
    base::BigEndianReader* reader) {

  while (reader->remaining()) {
    uint32_t truncated_rtp_timestamp;
    uint32_t data;
    if (!reader->ReadU32(&truncated_rtp_timestamp) || !reader->ReadU32(&data))
      return false;

    // We have 24 LSB of the event timestamp base on the wire.
    base::TimeTicks event_timestamp_base =
        base::TimeTicks() + base::Milliseconds(data & 0xffffff);

    size_t num_events = 1 + static_cast<uint8_t>(data >> 24);

    const RtpTimeTicks frame_log_rtp_timestamp =
        last_parsed_frame_log_rtp_timestamp_.Expand(truncated_rtp_timestamp);
    RtcpReceiverFrameLogMessage frame_log(frame_log_rtp_timestamp);
    for (size_t event = 0; event < num_events; event++) {
      uint16_t delay_delta_or_packet_id;
      uint16_t event_type_and_timestamp_delta;
      if (!reader->ReadU16(&delay_delta_or_packet_id) ||
          !reader->ReadU16(&event_type_and_timestamp_delta))
        return false;

      RtcpReceiverEventLogMessage event_log;
      event_log.type = TranslateToLogEventFromWireFormat(
          static_cast<uint8_t>(event_type_and_timestamp_delta >> 12));
      event_log.event_timestamp =
          event_timestamp_base +
          base::Milliseconds(event_type_and_timestamp_delta & 0xfff);
      if (event_log.type == PACKET_RECEIVED) {
        event_log.packet_id = delay_delta_or_packet_id;
      } else {
        event_log.delay_delta =
            base::Milliseconds(static_cast<int16_t>(delay_delta_or_packet_id));
      }
      frame_log.event_log_messages_.push_back(event_log);
    }

    last_parsed_frame_log_rtp_timestamp_ = frame_log_rtp_timestamp;
    receiver_log_.push_back(frame_log);
  }

  return true;
}

// RFC 4585.
bool RtcpParser::ParseFeedbackCommon(base::BigEndianReader* reader,
                                     const RtcpCommonHeader& header) {
  // The client must provide, up-front, a reference point for expanding the
  // truncated frame ID values.  If missing, it does not intend to process Cast
  // Feedback messages, so just return early.
  if (max_valid_frame_id_.is_null()) {
    return true;
  }

  // See RTC 4585 Section 6.4 for application specific feedback messages.
  if (header.IC != 15) {
    return true;
  }
  uint32_t remote_ssrc;
  uint32_t media_ssrc;
  if (!reader->ReadU32(&remote_ssrc) ||
      !reader->ReadU32(&media_ssrc))
    return false;

  if (remote_ssrc != remote_ssrc_)
    return true;

  uint32_t name;
  if (!reader->ReadU32(&name))
    return false;

  if (name != kCast) {
    return true;
  }

  cast_message_.remote_ssrc = remote_ssrc;

  uint8_t truncated_last_frame_id;
  uint8_t number_of_lost_fields;
  if (!reader->ReadU8(&truncated_last_frame_id) ||
      !reader->ReadU8(&number_of_lost_fields) ||
      !reader->ReadU16(&cast_message_.target_delay_ms))
    return false;

  cast_message_.ack_frame_id =
      max_valid_frame_id_.ExpandLessThanOrEqual(truncated_last_frame_id);

  cast_message_.missing_frames_and_packets.clear();
  cast_message_.received_later_frames.clear();
  for (size_t i = 0; i < number_of_lost_fields; i++) {
    uint8_t truncated_frame_id;
    uint16_t packet_id;
    uint8_t bitmask;
    if (!reader->ReadU8(&truncated_frame_id) || !reader->ReadU16(&packet_id) ||
        !reader->ReadU8(&bitmask))
      return false;
    const FrameId frame_id =
        cast_message_.ack_frame_id.Expand(truncated_frame_id);
    PacketIdSet& missing_packets =
        cast_message_.missing_frames_and_packets[frame_id];
    missing_packets.insert(packet_id);
    if (packet_id != kRtcpCastAllPacketsLost) {
      while (bitmask) {
        packet_id++;
        if (bitmask & 1)
          missing_packets.insert(packet_id);
        bitmask >>= 1;
      }
    }
  }

  has_cast_message_ = true;

  // Parse the extended feedback (Cst2). Ignore it if any error occurs.
  if ((!reader->ReadU32(&name)) || (name != kCst2))
    return true;
  if (!reader->ReadU8(&cast_message_.feedback_count))
    return true;
  uint8_t number_of_receiving_fields;
  if (!reader->ReadU8(&number_of_receiving_fields))
    return true;
  FrameId starting_frame_id = cast_message_.ack_frame_id + 2;
  for (size_t i = 0; i < number_of_receiving_fields; ++i) {
    uint8_t bitmask;
    if (!reader->ReadU8(&bitmask))
      return true;
    FrameId frame_id = starting_frame_id;
    while (bitmask) {
      if (bitmask & 1)
        cast_message_.received_later_frames.push_back(frame_id);
      ++frame_id;
      bitmask >>= 1;
    }
    starting_frame_id += 8;
  }

  has_cst2_message_ = true;
  return true;
}

bool RtcpParser::ParseExtendedReport(base::BigEndianReader* reader,
                                     const RtcpCommonHeader& header) {
  uint32_t remote_ssrc;
  if (!reader->ReadU32(&remote_ssrc))
    return false;

  // Is it for us?
  if (remote_ssrc != remote_ssrc_)
    return true;

  while (reader->remaining()) {
    uint8_t block_type;
    uint16_t block_length;
    if (!reader->ReadU8(&block_type) ||
        !reader->Skip(1) ||
        !reader->ReadU16(&block_length))
      return false;

    switch (block_type) {
      case 4:  // RRTR. RFC3611 Section 4.4.
        if (block_length != 2)
          return false;
        if (!ParseExtendedReportReceiverReferenceTimeReport(reader,
                                                            remote_ssrc))
          return false;
        break;

      default:
        // Skip unknown item.
        if (!reader->Skip(block_length * 4))
          return false;
    }
  }

  return true;
}

bool RtcpParser::ParseExtendedReportReceiverReferenceTimeReport(
    base::BigEndianReader* reader,
    uint32_t remote_ssrc) {
  receiver_reference_time_report_.remote_ssrc = remote_ssrc;
  if (!reader->ReadU32(&receiver_reference_time_report_.ntp_seconds) ||
      !reader->ReadU32(&receiver_reference_time_report_.ntp_fraction))
    return false;

  has_receiver_reference_time_report_ = true;
  return true;
}

// Converts a log event type to an integer value.
// NOTE: We have only allocated 4 bits to represent the type of event over the
// wire. Therefore, this function can only return values from 0 to 15.
uint8_t ConvertEventTypeToWireFormat(CastLoggingEvent event) {
  switch (event) {
    case FRAME_ACK_SENT:
      return 11;
    case FRAME_PLAYOUT:
      return 12;
    case FRAME_DECODED:
      return 13;
    case PACKET_RECEIVED:
      return 14;
    default:
      return 0;  // Not an interesting event.
  }
}

CastLoggingEvent TranslateToLogEventFromWireFormat(uint8_t event) {
  // TODO(imcheng): Remove the old mappings once they are no longer used.
  switch (event) {
    case 1:  // AudioAckSent
    case 5:  // VideoAckSent
    case 11:  // Unified
      return FRAME_ACK_SENT;
    case 2:  // AudioPlayoutDelay
    case 7:  // VideoRenderDelay
    case 12:  // Unified
      return FRAME_PLAYOUT;
    case 3:  // AudioFrameDecoded
    case 6:  // VideoFrameDecoded
    case 13:  // Unified
      return FRAME_DECODED;
    case 4:  // AudioPacketReceived
    case 8:  // VideoPacketReceived
    case 14:  // Unified
      return PACKET_RECEIVED;
    case 9:  // DuplicateAudioPacketReceived
    case 10:  // DuplicateVideoPacketReceived
    default:
      // If the sender adds new log messages we will end up here until we add
      // the new messages in the receiver.
      VLOG(1) << "Unexpected log message received: " << static_cast<int>(event);
      return UNKNOWN;
  }
}

void ConvertTimeToFractions(int64_t ntp_time_us,
                            uint32_t* seconds,
                            uint32_t* fractions) {
  DCHECK_GE(ntp_time_us, 0) << "Time must NOT be negative";
  const int64_t seconds_component =
      ntp_time_us / base::Time::kMicrosecondsPerSecond;
  // NTP time will overflow in the year 2036.  Also, make sure unit tests don't
  // regress and use an origin past the year 2036.  If this overflows here, the
  // inverse calculation fails to compute the correct TimeTicks value, throwing
  // off the entire system.
  DCHECK_LT(seconds_component, INT64_C(4263431296))
      << "One year left to fix the NTP year 2036 wrap-around issue!";
  *seconds = static_cast<uint32_t>(seconds_component);
  *fractions =
      static_cast<uint32_t>((ntp_time_us % base::Time::kMicrosecondsPerSecond) *
                            kMagicFractionalUnit);
}

void ConvertTimeTicksToNtp(const base::TimeTicks& time,
                           uint32_t* ntp_seconds,
                           uint32_t* ntp_fractions) {
  base::TimeDelta elapsed_since_unix_epoch =
      time - base::TimeTicks::UnixEpoch();

  int64_t ntp_time_us =
      elapsed_since_unix_epoch.InMicroseconds() +
      (kUnixEpochInNtpSeconds * base::Time::kMicrosecondsPerSecond);

  ConvertTimeToFractions(ntp_time_us, ntp_seconds, ntp_fractions);
}

base::TimeTicks ConvertNtpToTimeTicks(uint32_t ntp_seconds,
                                      uint32_t ntp_fractions) {
  // We need to ceil() here because the calculation of |fractions| in
  // ConvertTimeToFractions() effectively does a floor().
  int64_t ntp_time_us =
      ntp_seconds * base::Time::kMicrosecondsPerSecond +
      static_cast<int64_t>(std::ceil(ntp_fractions / kMagicFractionalUnit));

  base::TimeDelta elapsed_since_unix_epoch =
      base::Microseconds(ntp_time_us - (kUnixEpochInNtpSeconds *
                                        base::Time::kMicrosecondsPerSecond));
  return base::TimeTicks::UnixEpoch() + elapsed_since_unix_epoch;
}

uint32_t ConvertToNtpDiff(uint32_t delay_seconds, uint32_t delay_fraction) {
  return ((delay_seconds & 0x0000FFFF) << 16) +
         ((delay_fraction & 0xFFFF0000) >> 16);
}

namespace {
enum {
  // Minimum number of bytes required to make a valid RTCP packet.
  kMinLengthOfRtcp = 8,
};
}  // namespace

bool IsRtcpPacket(const uint8_t* packet, size_t length) {
  if (length < kMinLengthOfRtcp) {
    LOG(ERROR) << "Invalid RTCP packet received.";
    return false;
  }

  uint8_t packet_type = packet[1];
  return packet_type >= kPacketTypeLow && packet_type <= kPacketTypeHigh;
}

uint32_t GetSsrcOfSender(const uint8_t* rtcp_buffer, size_t length) {
  if (length < kMinLengthOfRtcp)
    return 0;
  uint32_t ssrc_of_sender;
  base::BigEndianReader big_endian_reader(rtcp_buffer, length);
  big_endian_reader.Skip(4);  // Skip header.
  big_endian_reader.ReadU32(&ssrc_of_sender);
  return ssrc_of_sender;
}

}  // namespace cast
}  // namespace media
