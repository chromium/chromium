// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtp/rtp_packetizer.h"

#include <string>

#include "base/big_endian.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "media/cast/common/encoded_frame.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/net/rtp/rtp_defines.h"
#include "third_party/openscreen/src/cast/streaming/encoded_frame.h"

namespace media {
namespace cast {

namespace {

// IP headers are 20 bytes, and ICMP headers are 8 bytes.
constexpr uint16_t kIpAndIcmpHeadersLength = 28;

// We may add an additional header to the first packet to update the playout
// delay.
constexpr uint16_t kPlayoutDelayHeaderLength = 4;

// We always include the playout delay in our header length. This does result
// in *slightly* suboptimal behavior in some edge cases, as we have a slightly
// lower maximum packet size.
constexpr uint16_t kHeadersLength =
    kRtpHeaderLength + kCastHeaderLength + kPlayoutDelayHeaderLength;

// The default max payload size is the maximum transmission unit
// |kMaxIpPacketSize| minus all of the lower level headers (IP and ICMP
// specifically).
constexpr uint16_t kDefaultMaxPayloadLength =
    kMaxIpPacketSize - kIpAndIcmpHeadersLength;

}  // namespace

RtpPacketizerConfig::RtpPacketizerConfig()
    : payload_type(-1),
      max_payload_length(kDefaultMaxPayloadLength),
      sequence_number(0),
      ssrc(0) {}

RtpPacketizerConfig::~RtpPacketizerConfig() = default;

RtpPacketizer::RtpPacketizer(PacedSender* const transport,
                             PacketStorage* packet_storage,
                             RtpPacketizerConfig rtp_packetizer_config)
    : config_(rtp_packetizer_config),
      transport_(transport),
      packet_storage_(packet_storage),
      sequence_number_(config_.sequence_number),
      send_packet_count_(0),
      send_octet_count_(0) {
  DCHECK(transport) << "Invalid argument";
}

RtpPacketizer::~RtpPacketizer() = default;

uint16_t RtpPacketizer::NextSequenceNumber() {
  ++sequence_number_;
  return sequence_number_ - 1;
}

void RtpPacketizer::SendFrameAsPackets(const EncodedFrame& frame) {
  // We don't packetize empty packets.
  if (frame.data.empty())
    return;

  const uint16_t max_length = config_.max_payload_length - kHeadersLength;
  // Split the payload evenly (round number up).
  size_t num_packets = (frame.data.size() + max_length - 1) / max_length;
  DCHECK_LE(num_packets, std::numeric_limits<uint16_t>::max());
  size_t payload_length = (frame.data.size() + num_packets - 1) / num_packets;
  DCHECK_LE(payload_length, max_length);

  SendPacketVector packets;

  size_t remaining_size = frame.data.size();
  std::string::const_iterator data_iter = frame.data.begin();

  uint8_t num_extensions = 0;
  if (frame.new_playout_delay_ms)
    num_extensions++;
  DCHECK_LE(num_extensions, kCastExtensionCountmask);

  while (remaining_size > 0) {
    PacketRef packet(new base::RefCountedData<Packet>);

    if (remaining_size < payload_length) {
      payload_length = remaining_size;
    }
    remaining_size -= payload_length;
    BuildCommonRtpHeader(&packet->data, remaining_size == 0,
                         frame.rtp_timestamp);

    // Build Cast header.
    DCHECK_NE(frame.dependency,
              openscreen::cast::EncodedFrame::Dependency::kUnknown);
    uint8_t byte0 = kCastReferenceFrameIdBitMask;
    if (frame.dependency ==
        openscreen::cast::EncodedFrame::Dependency::kKeyFrame)
      byte0 |= kCastKeyFrameBitMask;

    // Extensions only go on the first packet of the frame
    const uint16_t packet_id = static_cast<uint16_t>(packets.size());
    if (packet_id == 0)
      byte0 |= num_extensions;

    packet->data.push_back(byte0);
    packet->data.push_back(frame.frame_id.lower_8_bits());
    size_t start_size = packet->data.size();
    packet->data.resize(start_size + 4);
    base::BigEndianWriter big_endian_writer(
        reinterpret_cast<char*>(&(packet->data[start_size])), 4);
    big_endian_writer.WriteU16(packet_id);
    big_endian_writer.WriteU16(static_cast<uint16_t>(num_packets - 1));
    packet->data.push_back(frame.referenced_frame_id.lower_8_bits());

    // Add extension details only on the first packet of the frame.
    if (packet_id == 0 && frame.new_playout_delay_ms) {
      packet->data.push_back(kCastRtpExtensionAdaptiveLatency << 2);
      packet->data.push_back(2);  // 2 bytes
      packet->data.push_back(
          static_cast<uint8_t>(frame.new_playout_delay_ms >> 8));
      packet->data.push_back(static_cast<uint8_t>(frame.new_playout_delay_ms));
    }
    DCHECK_LE(packet->data.size(), kHeadersLength);

    // Copy payload data.
    packet->data.insert(packet->data.end(),
                        data_iter,
                        data_iter + payload_length);
    data_iter += payload_length;

    // If packets are too large, meaning that the payload size plus all headers
    // is larger than the MTU size of the network, this packet will get
    // fragmented.
    DCHECK(packet->data.size() <= config_.max_payload_length);
    packets.push_back(make_pair(PacketKey(frame.reference_time, config_.ssrc,
                                          frame.frame_id, packet_id),
                                packet));

    // Update stats.
    ++send_packet_count_;
    send_octet_count_ += payload_length;
  }
  DCHECK_EQ(num_packets, packets.size());

  packet_storage_->StoreFrame(frame.frame_id, packets);

  // Send to network.
  transport_->SendPackets(packets);
}

void RtpPacketizer::BuildCommonRtpHeader(Packet* packet,
                                         bool marker_bit,
                                         RtpTimeTicks rtp_timestamp) {
  packet->push_back(0x80);
  packet->push_back(static_cast<uint8_t>(config_.payload_type) |
                    (marker_bit ? kRtpMarkerBitMask : 0));
  const size_t start_size = packet->size();
  packet->resize(start_size + 10);
  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 10);
  big_endian_writer.WriteU16(sequence_number_);
  big_endian_writer.WriteU32(rtp_timestamp.lower_32_bits());
  big_endian_writer.WriteU32(config_.ssrc);
  ++sequence_number_;
}

}  // namespace cast
}  // namespace media
