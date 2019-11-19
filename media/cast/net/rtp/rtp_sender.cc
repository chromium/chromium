// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtp/rtp_sender.h"

#include "base/big_endian.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "media/cast/constants.h"

namespace media {
namespace cast {

namespace {

// If there is only one referecne to the packet then copy the
// reference and return.
// Otherwise return a deep copy of the packet.
PacketRef FastCopyPacket(const PacketRef& packet) {
  if (packet->HasOneRef())
    return packet;
  return base::WrapRefCounted(new base::RefCountedData<Packet>(packet->data));
}

}  // namespace

RtpSender::RtpSender(
    const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner,
    PacedSender* const transport)
    : transport_(transport), transport_task_runner_(transport_task_runner) {
  // Randomly set sequence number start value.
  config_.sequence_number = base::RandInt(0, 65535);
}

RtpSender::~RtpSender() = default;

bool RtpSender::Initialize(const CastTransportRtpConfig& config) {
  config_.ssrc = config.ssrc;
  // TODO(xjz): Android TV receivers expect the |payload_type| to be one of
  // these two specific values. This constraint needs to be removed and the
  // value of the |payload_type| can vary according to the spec:
  // https://tools.ietf.org/html/rfc3551.
  if (config.rtp_payload_type <= RtpPayloadType::AUDIO_LAST)
    config_.payload_type = 127;
  else
    config_.payload_type = 96;
  packetizer_.reset(new RtpPacketizer(transport_, &storage_, config_));
  return true;
}

void RtpSender::SendFrame(const EncodedFrame& frame) {
  DCHECK(packetizer_);
  packetizer_->SendFrameAsPackets(frame);
  LOG_IF(DFATAL, storage_.GetNumberOfStoredFrames() > kMaxUnackedFrames)
      << "Possible bug: Frames are not being actively released from storage.";
}

void RtpSender::ResendPackets(
    const MissingFramesAndPacketsMap& missing_frames_and_packets,
    bool cancel_rtx_if_not_in_list, const DedupInfo& dedup_info) {
  // Iterate over all frames in the list.
  for (auto it = missing_frames_and_packets.begin();
       it != missing_frames_and_packets.end(); ++it) {
    SendPacketVector packets_to_resend;
    FrameId frame_id = it->first;
    // Set of packets that the receiver wants us to re-send.
    // If empty, we need to re-send all packets for this frame.
    const PacketIdSet& missing_packet_set = it->second;

    bool resend_all = missing_packet_set.find(kRtcpCastAllPacketsLost) !=
        missing_packet_set.end();
    bool resend_last = missing_packet_set.find(kRtcpCastLastPacket) !=
        missing_packet_set.end();

    const SendPacketVector* stored_packets = storage_.GetFramePackets(frame_id);
    if (!stored_packets)
      continue;

    for (auto it = stored_packets->begin(); it != stored_packets->end(); ++it) {
      const PacketKey& packet_key = it->first;
      const uint16_t packet_id = packet_key.packet_id;

      // Should we resend the packet?
      bool resend = resend_all;

      // Should we resend it because it's in the missing_packet_set?
      if (!resend &&
          missing_packet_set.find(packet_id) != missing_packet_set.end()) {
        resend = true;
      }

      // If we were asked to resend the last packet, check if it's the
      // last packet.
      if (!resend && resend_last && (it + 1) == stored_packets->end()) {
        resend = true;
      }

      if (resend) {
        // Resend packet to the network.
        VLOG(3) << "Resend " << frame_id << ":" << packet_id;
        // Set a unique incremental sequence number for every packet.
        PacketRef packet_copy = FastCopyPacket(it->second);
        UpdateSequenceNumber(&packet_copy->data);
        packets_to_resend.push_back(std::make_pair(packet_key, packet_copy));
      } else if (cancel_rtx_if_not_in_list) {
        transport_->CancelSendingPacket(it->first);
      }
    }
    transport_->ResendPackets(packets_to_resend, dedup_info);
  }
}

void RtpSender::CancelSendingFrames(const std::vector<FrameId>& frame_ids) {
  for (FrameId i : frame_ids) {
    const SendPacketVector* stored_packets = storage_.GetFramePackets(i);
    if (!stored_packets)
      continue;
    for (auto j = stored_packets->begin(); j != stored_packets->end(); ++j) {
      transport_->CancelSendingPacket(j->first);
    }
    storage_.ReleaseFrame(i);
  }
}

void RtpSender::ResendFrameForKickstart(FrameId frame_id,
                                        base::TimeDelta dedupe_window) {
  // Send the last packet of the encoded frame to kick start
  // retransmission. This gives enough information to the receiver what
  // packets and frames are missing.
  MissingFramesAndPacketsMap missing_frames_and_packets;
  PacketIdSet missing;
  missing.insert(kRtcpCastLastPacket);
  missing_frames_and_packets.insert(std::make_pair(frame_id, missing));

  // Sending this extra packet is to kick-start the session. There is
  // no need to optimize re-transmission for this case.
  DedupInfo dedup_info;
  dedup_info.resend_interval = dedupe_window;
  ResendPackets(missing_frames_and_packets, false, dedup_info);
}

void RtpSender::UpdateSequenceNumber(Packet* packet) {
  // TODO(miu): This is an abstraction violation.  This needs to be a part of
  // the overall packet (de)serialization consolidation.
  static const int kByteOffsetToSequenceNumber = 2;
  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>((&packet->front()) + kByteOffsetToSequenceNumber),
      sizeof(uint16_t));
  big_endian_writer.WriteU16(packetizer_->NextSequenceNumber());
}

int64_t RtpSender::GetLastByteSentForFrame(FrameId frame_id) {
  const SendPacketVector* stored_packets = storage_.GetFramePackets(frame_id);
  if (!stored_packets)
    return 0;
  PacketKey last_packet_key = stored_packets->rbegin()->first;
  return transport_->GetLastByteSentForPacket(last_packet_key);
}

}  //  namespace cast
}  // namespace media
