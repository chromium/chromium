// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtcp/rtcp_builder.h"

#include <stdint.h>

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "media/cast/net/rtcp/rtcp_utility.h"

namespace media {
namespace cast {
namespace {

// Max delta is 4095 milliseconds because we need to be able to encode it in
// 12 bits.
const int64_t kMaxWireFormatTimeDeltaMs = INT64_C(0xfff);

uint16_t MergeEventTypeAndTimestampForWireFormat(
    const CastLoggingEvent& event,
    const base::TimeDelta& time_delta) {
  int64_t time_delta_ms = time_delta.InMilliseconds();

  DCHECK_GE(time_delta_ms, 0);
  DCHECK_LE(time_delta_ms, kMaxWireFormatTimeDeltaMs);

  uint16_t time_delta_12_bits =
      static_cast<uint16_t>(time_delta_ms & kMaxWireFormatTimeDeltaMs);

  uint16_t event_type_4_bits = ConvertEventTypeToWireFormat(event);
  DCHECK(event_type_4_bits);
  DCHECK(~(event_type_4_bits & 0xfff0));
  return (event_type_4_bits << 12) | time_delta_12_bits;
}

bool EventTimestampLessThan(const RtcpReceiverEventLogMessage& lhs,
                            const RtcpReceiverEventLogMessage& rhs) {
  return lhs.event_timestamp < rhs.event_timestamp;
}

// A class to build a string representing the NACK list in Cast message.
//
// The string will look like "F23:3-6 F25:1,5-6", meaning packets 3 to 6 in
// frame 23 are being NACK'ed (i.e. they are missing from the receiver's point
// of view) and packets 1, 5 and 6 are missing in frame 25. A frame that is
// completely missing will show as "F26:65535".
class NackStringBuilder {
 public:
  NackStringBuilder()
      : frame_count_(0),
        packet_count_(0),
        last_packet_id_(-1),
        contiguous_sequence_(false) {}
  ~NackStringBuilder() = default;

  bool Empty() const { return frame_count_ == 0; }

  void PushFrame(FrameId frame_id) {
    DCHECK(!frame_id.is_null());
    if (frame_count_ > 0) {
      if (frame_id == last_frame_id_) {
        return;
      }
      if (contiguous_sequence_) {
        stream_ << "-" << last_packet_id_;
      }
      stream_ << ", ";
    }
    stream_ << frame_id;
    last_frame_id_ = frame_id;
    packet_count_ = 0;
    contiguous_sequence_ = false;
    ++frame_count_;
  }

  void PushPacket(int packet_id) {
    DCHECK(!last_frame_id_.is_null());
    DCHECK_GE(packet_id, 0);
    if (packet_count_ == 0) {
      stream_ << ":" << packet_id;
    } else if (packet_id == last_packet_id_ + 1) {
      contiguous_sequence_ = true;
    } else {
      if (contiguous_sequence_) {
        stream_ << "-" << last_packet_id_;
        contiguous_sequence_ = false;
      }
      stream_ << "," << packet_id;
    }
    ++packet_count_;
    last_packet_id_ = packet_id;
  }

  std::string GetString() {
    if (contiguous_sequence_) {
      stream_ << "-" << last_packet_id_;
      contiguous_sequence_ = false;
    }
    return stream_.str();
  }

 private:
  std::ostringstream stream_;
  int frame_count_;
  int packet_count_;
  FrameId last_frame_id_;
  int last_packet_id_;
  bool contiguous_sequence_;
};
}  // namespace

RtcpBuilder::RtcpBuilder(uint32_t sending_ssrc)
    : writer_(NULL, 0), local_ssrc_(sending_ssrc), ptr_of_length_(NULL) {}

RtcpBuilder::~RtcpBuilder() = default;

void RtcpBuilder::PatchLengthField() {
  if (ptr_of_length_) {
    // Back-patch the packet length. The client must have taken
    // care of proper padding to 32-bit words.
    int this_packet_length = (writer_.ptr() - ptr_of_length_ - 2);
    DCHECK_EQ(0, this_packet_length % 4)
        << "Packets must be a multiple of 32 bits long";
    *ptr_of_length_ = this_packet_length >> 10;
    *(ptr_of_length_ + 1) = (this_packet_length >> 2) & 0xFF;
    ptr_of_length_ = NULL;
  }
}

// Set the 5-bit value in the 1st byte of the header
// and the payload type. Set aside room for the length field,
// and make provision for back-patching it.
void RtcpBuilder::AddRtcpHeader(RtcpPacketFields payload, int format_or_count) {
  PatchLengthField();
  writer_.WriteU8(0x80 | (format_or_count & 0x1F));
  writer_.WriteU8(payload);
  ptr_of_length_ = writer_.ptr();

  // Initialize length to "clearly illegal".
  writer_.WriteU16(0xDEAD);
}

void RtcpBuilder::Start() {
  packet_ = new base::RefCountedData<Packet>;
  packet_->data.resize(kMaxIpPacketSize);
  writer_ = base::BigEndianWriter(
      reinterpret_cast<char*>(&(packet_->data[0])), kMaxIpPacketSize);
}

PacketRef RtcpBuilder::Finish() {
  PatchLengthField();
  packet_->data.resize(kMaxIpPacketSize - writer_.remaining());
  writer_ = base::BigEndianWriter(NULL, 0);
  PacketRef ret = packet_;
  packet_.reset();
  return ret;
}

PacketRef RtcpBuilder::BuildRtcpFromSender(const RtcpSenderInfo& sender_info) {
  Start();
  AddSR(sender_info);
  return Finish();
}

void RtcpBuilder::AddRR(const RtcpReportBlock* report_block) {
  AddRtcpHeader(kPacketTypeReceiverReport, report_block ? 1 : 0);
  writer_.WriteU32(local_ssrc_);
  if (report_block) {
    AddReportBlocks(*report_block);  // Adds 24 bytes.
  }
}

void RtcpBuilder::AddReportBlocks(const RtcpReportBlock& report_block) {
  writer_.WriteU32(report_block.media_ssrc);
  writer_.WriteU8(report_block.fraction_lost);
  writer_.WriteU8(report_block.cumulative_lost >> 16);
  writer_.WriteU8(report_block.cumulative_lost >> 8);
  writer_.WriteU8(report_block.cumulative_lost);

  // Extended highest seq_no, contain the highest sequence number received.
  writer_.WriteU32(report_block.extended_high_sequence_number);
  writer_.WriteU32(report_block.jitter);

  // Last SR timestamp; our NTP time when we received the last report.
  // This is the value that we read from the send report packet not when we
  // received it.
  writer_.WriteU32(report_block.last_sr);

  // Delay since last received report, time since we received the report.
  writer_.WriteU32(report_block.delay_since_last_sr);
}

void RtcpBuilder::AddRrtr(const RtcpReceiverReferenceTimeReport& rrtr) {
  AddRtcpHeader(kPacketTypeXr, 0);
  writer_.WriteU32(local_ssrc_);  // Add our own SSRC.
  writer_.WriteU8(4);       // Add block type.
  writer_.WriteU8(0);       // Add reserved.
  writer_.WriteU16(2);      // Block length.

  // Add the media (received RTP) SSRC.
  writer_.WriteU32(rrtr.ntp_seconds);
  writer_.WriteU32(rrtr.ntp_fraction);
}

void RtcpBuilder::AddPli(const RtcpPliMessage& pli_message) {
  AddRtcpHeader(kPacketTypePayloadSpecific, 1);
  writer_.WriteU32(local_ssrc_);
  writer_.WriteU32(pli_message.remote_ssrc);
}

void RtcpBuilder::AddCast(const RtcpCastMessage& cast,
                          base::TimeDelta target_delay) {
  // See RTC 4585 Section 6.4 for application specific feedback messages.
  AddRtcpHeader(kPacketTypePayloadSpecific, 15);
  writer_.WriteU32(local_ssrc_);      // Add our own SSRC.
  writer_.WriteU32(cast.remote_ssrc);  // Remote SSRC.
  writer_.WriteU32(kCast);
  writer_.WriteU8(cast.ack_frame_id.lower_8_bits());
  uint8_t* cast_loss_field_pos = reinterpret_cast<uint8_t*>(writer_.ptr());
  writer_.WriteU8(0);  // Overwritten with number_of_loss_fields.
  DCHECK_LE(target_delay.InMilliseconds(),
            std::numeric_limits<uint16_t>::max());
  writer_.WriteU16(target_delay.InMilliseconds());

  size_t number_of_loss_fields = 0;
  size_t max_number_of_loss_fields = std::min<size_t>(
      kRtcpMaxCastLossFields, writer_.remaining() / 4);

  auto frame_it = cast.missing_frames_and_packets.begin();

  NackStringBuilder nack_string_builder;
  for (; frame_it != cast.missing_frames_and_packets.end() &&
         number_of_loss_fields < max_number_of_loss_fields;
       ++frame_it) {
    nack_string_builder.PushFrame(frame_it->first);
    // Iterate through all frames with missing packets.
    if (frame_it->second.empty()) {
      // Special case all packets in a frame is missing.
      writer_.WriteU8(frame_it->first.lower_8_bits());
      writer_.WriteU16(kRtcpCastAllPacketsLost);
      writer_.WriteU8(0);
      nack_string_builder.PushPacket(kRtcpCastAllPacketsLost);
      ++number_of_loss_fields;
    } else {
      auto packet_it = frame_it->second.begin();
      while (packet_it != frame_it->second.end()) {
        uint16_t packet_id = *packet_it;
        // Write frame and packet id to buffer before calculating bitmask.
        writer_.WriteU8(frame_it->first.lower_8_bits());
        writer_.WriteU16(packet_id);
        nack_string_builder.PushPacket(packet_id);

        uint8_t bitmask = 0;
        ++packet_it;
        while (packet_it != frame_it->second.end()) {
          int shift = static_cast<uint8_t>(*packet_it - packet_id) - 1;
          if (shift >= 0 && shift <= 7) {
            nack_string_builder.PushPacket(*packet_it);
            bitmask |= (1 << shift);
            ++packet_it;
          } else {
            break;
          }
        }
        writer_.WriteU8(bitmask);
        ++number_of_loss_fields;
      }
    }
  }
  VLOG_IF(1, !nack_string_builder.Empty())
      << "SSRC: " << cast.remote_ssrc << ", ACK: " << cast.ack_frame_id
      << ", NACK: " << nack_string_builder.GetString();
  DCHECK_LE(number_of_loss_fields, kRtcpMaxCastLossFields);
  *cast_loss_field_pos = static_cast<uint8_t>(number_of_loss_fields);
}

void RtcpBuilder::AddSR(const RtcpSenderInfo& sender_info) {
  AddRtcpHeader(kPacketTypeSenderReport, 0);
  writer_.WriteU32(local_ssrc_);
  writer_.WriteU32(sender_info.ntp_seconds);
  writer_.WriteU32(sender_info.ntp_fraction);
  writer_.WriteU32(sender_info.rtp_timestamp.lower_32_bits());
  writer_.WriteU32(sender_info.send_packet_count);
  writer_.WriteU32(static_cast<uint32_t>(sender_info.send_octet_count));
}

/*
   0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |V=2|P|reserved |   PT=XR=207   |             length            |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                              SSRC                             |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |     BT=5      |   reserved    |         block length          |
  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  |                 SSRC1 (SSRC of first receiver)               | sub-
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
  |                         last RR (LRR)                         |   1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                   delay since last RR (DLRR)                  |
  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*/
void RtcpBuilder::AddDlrrRb(const RtcpDlrrReportBlock& dlrr) {
  AddRtcpHeader(kPacketTypeXr, 0);
  writer_.WriteU32(local_ssrc_);  // Add our own SSRC.
  writer_.WriteU8(5);  // Add block type.
  writer_.WriteU8(0);  // Add reserved.
  writer_.WriteU16(3);  // Block length.
  writer_.WriteU32(local_ssrc_);  // Add the media (received RTP) SSRC.
  writer_.WriteU32(dlrr.last_rr);
  writer_.WriteU32(dlrr.delay_since_last_rr);
}

void RtcpBuilder::AddReceiverLog(
    const ReceiverRtcpEventSubscriber::RtcpEvents& rtcp_events) {
  size_t total_number_of_messages_to_send = 0;
  RtcpReceiverLogMessage receiver_log_message;

  if (!GetRtcpReceiverLogMessage(rtcp_events,
                                 &receiver_log_message,
                                 &total_number_of_messages_to_send)) {
    return;
  }

  AddRtcpHeader(kPacketTypeApplicationDefined, kReceiverLogSubtype);
  writer_.WriteU32(local_ssrc_);  // Add our own SSRC.
  writer_.WriteU32(kCast);

  while (!receiver_log_message.empty() &&
         total_number_of_messages_to_send > 0) {
    RtcpReceiverFrameLogMessage& frame_log_messages(
        receiver_log_message.front());

    // Add our frame header.
    writer_.WriteU32(frame_log_messages.rtp_timestamp_.lower_32_bits());
    size_t messages_in_frame = frame_log_messages.event_log_messages_.size();
    if (messages_in_frame > total_number_of_messages_to_send) {
      // We are running out of space.
      messages_in_frame = total_number_of_messages_to_send;
    }
    // Keep track of how many messages we have left to send.
    total_number_of_messages_to_send -= messages_in_frame;

    // On the wire format is number of messages - 1.
    writer_.WriteU8(static_cast<uint8_t>(messages_in_frame - 1));

    base::TimeTicks event_timestamp_base =
        frame_log_messages.event_log_messages_.front().event_timestamp;
    uint32_t base_timestamp_ms =
        (event_timestamp_base - base::TimeTicks()).InMilliseconds();
    writer_.WriteU8(static_cast<uint8_t>(base_timestamp_ms >> 16));
    writer_.WriteU8(static_cast<uint8_t>(base_timestamp_ms >> 8));
    writer_.WriteU8(static_cast<uint8_t>(base_timestamp_ms));

    while (!frame_log_messages.event_log_messages_.empty() &&
           messages_in_frame > 0) {
      const RtcpReceiverEventLogMessage& event_message =
          frame_log_messages.event_log_messages_.front();
      uint16_t event_type_and_timestamp_delta =
          MergeEventTypeAndTimestampForWireFormat(
              event_message.type,
              event_message.event_timestamp - event_timestamp_base);
      switch (event_message.type) {
        case FRAME_ACK_SENT:
        case FRAME_PLAYOUT:
        case FRAME_DECODED:
          writer_.WriteU16(static_cast<uint16_t>(
              event_message.delay_delta.InMilliseconds()));
          writer_.WriteU16(event_type_and_timestamp_delta);
          break;
        case PACKET_RECEIVED:
          writer_.WriteU16(event_message.packet_id);
          writer_.WriteU16(event_type_and_timestamp_delta);
          break;
        default:
          NOTREACHED();
      }
      messages_in_frame--;
      frame_log_messages.event_log_messages_.pop_front();
    }
    if (frame_log_messages.event_log_messages_.empty()) {
      // We sent all messages on this frame; pop the frame header.
      receiver_log_message.pop_front();
    }
  }
  DCHECK_EQ(total_number_of_messages_to_send, 0u);
}

bool RtcpBuilder::GetRtcpReceiverLogMessage(
    const ReceiverRtcpEventSubscriber::RtcpEvents& rtcp_events,
    RtcpReceiverLogMessage* receiver_log_message,
    size_t* total_number_of_messages_to_send) {
  size_t number_of_frames = 0;
  size_t remaining_space = writer_.remaining();
  if (remaining_space < kRtcpCastLogHeaderSize + kRtcpReceiverFrameLogSize +
                            kRtcpReceiverEventLogSize) {
    return false;
  }

  // We use this to do event timestamp sorting and truncating for events of
  // a single frame.
  std::vector<RtcpReceiverEventLogMessage> sorted_log_messages;

  // Account for the RTCP header for an application-defined packet.
  remaining_space -= kRtcpCastLogHeaderSize;

  auto rit = rtcp_events.rbegin();

  while (rit != rtcp_events.rend() &&
         remaining_space >=
             kRtcpReceiverFrameLogSize + kRtcpReceiverEventLogSize) {
    const RtpTimeTicks rtp_timestamp = rit->first;
    RtcpReceiverFrameLogMessage frame_log(rtp_timestamp);
    remaining_space -= kRtcpReceiverFrameLogSize;
    ++number_of_frames;

    // Get all events of a single frame.
    sorted_log_messages.clear();
    do {
      RtcpReceiverEventLogMessage event_log_message;
      event_log_message.type = rit->second.type;
      event_log_message.event_timestamp = rit->second.timestamp;
      event_log_message.delay_delta = rit->second.delay_delta;
      event_log_message.packet_id = rit->second.packet_id;
      sorted_log_messages.push_back(event_log_message);
      ++rit;
    } while (rit != rtcp_events.rend() && rit->first == rtp_timestamp);

    std::sort(sorted_log_messages.begin(),
              sorted_log_messages.end(),
              &EventTimestampLessThan);

    // From |sorted_log_messages|, only take events that are no greater than
    // |kMaxWireFormatTimeDeltaMs| seconds away from the latest event. Events
    // older than that cannot be encoded over the wire.
    auto sorted_rit = sorted_log_messages.rbegin();
    base::TimeTicks first_event_timestamp = sorted_rit->event_timestamp;
    size_t events_in_frame = 0;
    while (sorted_rit != sorted_log_messages.rend() &&
           events_in_frame < kRtcpMaxReceiverLogMessages &&
           remaining_space >= kRtcpReceiverEventLogSize) {
      base::TimeDelta delta(first_event_timestamp -
                            sorted_rit->event_timestamp);
      if (delta.InMilliseconds() > kMaxWireFormatTimeDeltaMs)
        break;
      frame_log.event_log_messages_.push_front(*sorted_rit);
      ++events_in_frame;
      ++*total_number_of_messages_to_send;
      remaining_space -= kRtcpReceiverEventLogSize;
      ++sorted_rit;
    }

    receiver_log_message->push_front(frame_log);
  }

  VLOG(3) << "number of frames: " << number_of_frames;
  VLOG(3) << "total messages to send: " << *total_number_of_messages_to_send;
  return number_of_frames > 0;
}

}  // namespace cast
}  // namespace media
