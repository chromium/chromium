// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_packet_generator.h"

#include <cstdint>

#include "net/third_party/quic/core/crypto/quic_random.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quic/platform/api/quic_flag_utils.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"

namespace quic {

QuicPacketGenerator::QuicPacketGenerator(QuicConnectionId connection_id,
                                         QuicFramer* framer,
                                         QuicRandom* random_generator,
                                         DelegateInterface* delegate)
    : delegate_(delegate),
      packet_creator_(connection_id, framer, random_generator, delegate),
      flusher_attached_(false),
      should_send_ack_(false),
      should_send_stop_waiting_(false),
      random_generator_(random_generator) {}

QuicPacketGenerator::~QuicPacketGenerator() {
  DeleteFrames(&queued_control_frames_);
}

void QuicPacketGenerator::SetShouldSendAck(bool also_send_stop_waiting) {
  if (packet_creator_.has_ack()) {
    // Ack already queued, nothing to do.
    return;
  }

  if (also_send_stop_waiting && packet_creator_.has_stop_waiting()) {
    QUIC_BUG << "Should only ever be one pending stop waiting frame.";
    return;
  }

  should_send_ack_ = true;
  should_send_stop_waiting_ = also_send_stop_waiting;
  SendQueuedFrames(/*flush=*/false);
}

void QuicPacketGenerator::AddControlFrame(const QuicFrame& frame) {
  QUIC_BUG_IF(IsControlFrame(frame.type) && !GetControlFrameId(frame))
      << "Adding a control frame with no control frame id: " << frame;
  queued_control_frames_.push_back(frame);
  SendQueuedFrames(/*flush=*/false);
}

QuicConsumedData QuicPacketGenerator::ConsumeData(QuicStreamId id,
                                                  size_t write_length,
                                                  QuicStreamOffset offset,
                                                  StreamSendingState state) {
  QUIC_BUG_IF(!flusher_attached_) << "Packet flusher is not attached when "
                                     "generator tries to write stream data.";
  bool has_handshake =
      (id == QuicUtils::GetCryptoStreamId(packet_creator_.transport_version()));
  bool fin = state != NO_FIN;
  QUIC_BUG_IF(has_handshake && fin)
      << "Handshake packets should never send a fin";
  // To make reasoning about crypto frames easier, we don't combine them with
  // other retransmittable frames in a single packet.
  const bool flush =
      has_handshake && packet_creator_.HasPendingRetransmittableFrames();
  SendQueuedFrames(flush);

  size_t total_bytes_consumed = 0;
  bool fin_consumed = false;

  if (!packet_creator_.HasRoomForStreamFrame(id, offset, write_length)) {
    packet_creator_.Flush();
  }

  if (!fin && (write_length == 0)) {
    QUIC_BUG << "Attempt to consume empty data without FIN.";
    return QuicConsumedData(0, false);
  }
  // We determine if we can enter the fast path before executing
  // the slow path loop.
  bool run_fast_path = !has_handshake && state != FIN_AND_PADDING &&
                       !HasQueuedFrames() &&
                       write_length - total_bytes_consumed > kMaxPacketSize;

  while (!run_fast_path && delegate_->ShouldGeneratePacket(
                               HAS_RETRANSMITTABLE_DATA,
                               has_handshake ? IS_HANDSHAKE : NOT_HANDSHAKE)) {
    QuicFrame frame;
    if (!packet_creator_.ConsumeData(id, write_length, total_bytes_consumed,
                                     offset + total_bytes_consumed, fin,
                                     has_handshake, &frame)) {
      // The creator is always flushed if there's not enough room for a new
      // stream frame before ConsumeData, so ConsumeData should always succeed.
      QUIC_BUG << "Failed to ConsumeData, stream:" << id;
      return QuicConsumedData(0, false);
    }

    // A stream frame is created and added.
    size_t bytes_consumed = frame.stream_frame.data_length;
    total_bytes_consumed += bytes_consumed;
    fin_consumed = fin && total_bytes_consumed == write_length;
    if (fin_consumed && state == FIN_AND_PADDING) {
      AddRandomPadding();
    }
    DCHECK(total_bytes_consumed == write_length ||
           (bytes_consumed > 0 && packet_creator_.HasPendingFrames()));

    if (total_bytes_consumed == write_length) {
      // We're done writing the data. Exit the loop.
      // We don't make this a precondition because we could have 0 bytes of data
      // if we're simply writing a fin.
      break;
    }
    // TODO(ianswett): Move to having the creator flush itself when it's full.
    packet_creator_.Flush();

    run_fast_path = !has_handshake && state != FIN_AND_PADDING &&
                    !HasQueuedFrames() &&
                    write_length - total_bytes_consumed > kMaxPacketSize;
  }

  if (run_fast_path) {
    return ConsumeDataFastPath(id, write_length, offset, state != NO_FIN,
                               total_bytes_consumed);
  }

  // Don't allow the handshake to be bundled with other retransmittable frames.
  if (has_handshake) {
    SendQueuedFrames(/*flush=*/true);
  }

  return QuicConsumedData(total_bytes_consumed, fin_consumed);
}

QuicConsumedData QuicPacketGenerator::ConsumeDataFastPath(
    QuicStreamId id,
    size_t write_length,
    QuicStreamOffset offset,
    bool fin,
    size_t total_bytes_consumed) {
  DCHECK_NE(id,
            QuicUtils::GetCryptoStreamId(packet_creator_.transport_version()));

  while (total_bytes_consumed < write_length &&
         delegate_->ShouldGeneratePacket(HAS_RETRANSMITTABLE_DATA,
                                         NOT_HANDSHAKE)) {
    // Serialize and encrypt the packet.
    size_t bytes_consumed = 0;
    packet_creator_.CreateAndSerializeStreamFrame(
        id, write_length, total_bytes_consumed, offset + total_bytes_consumed,
        fin, &bytes_consumed);
    total_bytes_consumed += bytes_consumed;
  }

  return QuicConsumedData(total_bytes_consumed,
                          fin && (total_bytes_consumed == write_length));
}

void QuicPacketGenerator::GenerateMtuDiscoveryPacket(QuicByteCount target_mtu) {
  // MTU discovery frames must be sent by themselves.
  if (!packet_creator_.CanSetMaxPacketLength()) {
    QUIC_BUG << "MTU discovery packets should only be sent when no other "
             << "frames needs to be sent.";
    return;
  }
  const QuicByteCount current_mtu = GetCurrentMaxPacketLength();

  // The MTU discovery frame is allocated on the stack, since it is going to be
  // serialized within this function.
  QuicMtuDiscoveryFrame mtu_discovery_frame;
  QuicFrame frame(mtu_discovery_frame);

  // Send the probe packet with the new length.
  SetMaxPacketLength(target_mtu);
  const bool success = packet_creator_.AddPaddedSavedFrame(frame);
  packet_creator_.Flush();
  // The only reason AddFrame can fail is that the packet is too full to fit in
  // a ping.  This is not possible for any sane MTU.
  DCHECK(success);

  // Reset the packet length back.
  SetMaxPacketLength(current_mtu);
}

bool QuicPacketGenerator::CanSendWithNextPendingFrameAddition() const {
  DCHECK(HasPendingFrames() || packet_creator_.pending_padding_bytes() > 0);
  HasRetransmittableData retransmittable =
      (should_send_ack_ || should_send_stop_waiting_ ||
       packet_creator_.pending_padding_bytes() > 0)
          ? NO_RETRANSMITTABLE_DATA
          : HAS_RETRANSMITTABLE_DATA;
  if (retransmittable == HAS_RETRANSMITTABLE_DATA) {
    DCHECK(!queued_control_frames_.empty());  // These are retransmittable.
  }
  return delegate_->ShouldGeneratePacket(retransmittable, NOT_HANDSHAKE);
}

void QuicPacketGenerator::SendQueuedFrames(bool flush) {
  // Only add pending frames if we are SURE we can then send the whole packet.
  while (HasPendingFrames() &&
         (flush || CanSendWithNextPendingFrameAddition())) {
    bool first_frame = packet_creator_.CanSetMaxPacketLength();
    if (!AddNextPendingFrame() && first_frame) {
      // A single frame cannot fit into the packet, tear down the connection.
      QUIC_BUG << "A single frame cannot fit into packet."
               << " should_send_ack: " << should_send_ack_
               << " should_send_stop_waiting: " << should_send_stop_waiting_
               << " number of queued_control_frames: "
               << queued_control_frames_.size();
      if (!queued_control_frames_.empty()) {
        QUIC_LOG(INFO) << queued_control_frames_[0];
      }
      delegate_->OnUnrecoverableError(QUIC_FAILED_TO_SERIALIZE_PACKET,
                                      "Single frame cannot fit into a packet",
                                      ConnectionCloseSource::FROM_SELF);
      return;
    }
  }
  if (flush) {
    packet_creator_.Flush();
  }
}

bool QuicPacketGenerator::PacketFlusherAttached() const {
  return flusher_attached_;
}

void QuicPacketGenerator::AttachPacketFlusher() {
  flusher_attached_ = true;
}

void QuicPacketGenerator::Flush() {
  SendQueuedFrames(/*flush=*/false);
  packet_creator_.Flush();
  SendRemainingPendingPadding();
  flusher_attached_ = false;
}

void QuicPacketGenerator::FlushAllQueuedFrames() {
  SendQueuedFrames(/*flush=*/true);
}

bool QuicPacketGenerator::HasQueuedFrames() const {
  return packet_creator_.HasPendingFrames() || HasPendingFrames();
}

bool QuicPacketGenerator::IsPendingPacketEmpty() const {
  return !packet_creator_.HasPendingFrames();
}

bool QuicPacketGenerator::HasPendingFrames() const {
  return should_send_ack_ || should_send_stop_waiting_ ||
         !queued_control_frames_.empty();
}

bool QuicPacketGenerator::AddNextPendingFrame() {
  QUIC_BUG_IF(!flusher_attached_) << "Packet flusher is not attached when "
                                     "generator tries to write control frames.";
  if (should_send_ack_) {
    should_send_ack_ =
        !packet_creator_.AddSavedFrame(delegate_->GetUpdatedAckFrame());
    return !should_send_ack_;
  }

  if (should_send_stop_waiting_) {
    delegate_->PopulateStopWaitingFrame(&pending_stop_waiting_frame_);
    // If we can't this add the frame now, then we still need to do so later.
    should_send_stop_waiting_ =
        !packet_creator_.AddSavedFrame(QuicFrame(&pending_stop_waiting_frame_));
    // Return success if we have cleared out this flag (i.e., added the frame).
    // If we still need to send, then the frame is full, and we have failed.
    return !should_send_stop_waiting_;
  }

  QUIC_BUG_IF(queued_control_frames_.empty())
      << "AddNextPendingFrame called with no queued control frames.";
  if (!packet_creator_.AddSavedFrame(queued_control_frames_.back())) {
    // Packet was full.
    return false;
  }
  queued_control_frames_.pop_back();
  return true;
}

void QuicPacketGenerator::StopSendingVersion() {
  packet_creator_.StopSendingVersion();
}

void QuicPacketGenerator::SetDiversificationNonce(
    const DiversificationNonce& nonce) {
  packet_creator_.SetDiversificationNonce(nonce);
}

QuicPacketNumber QuicPacketGenerator::packet_number() const {
  return packet_creator_.packet_number();
}

QuicByteCount QuicPacketGenerator::GetCurrentMaxPacketLength() const {
  return packet_creator_.max_packet_length();
}

void QuicPacketGenerator::SetMaxPacketLength(QuicByteCount length) {
  DCHECK(packet_creator_.CanSetMaxPacketLength());
  packet_creator_.SetMaxPacketLength(length);
}

std::unique_ptr<QuicEncryptedPacket>
QuicPacketGenerator::SerializeVersionNegotiationPacket(
    bool ietf_quic,
    const ParsedQuicVersionVector& supported_versions) {
  return packet_creator_.SerializeVersionNegotiationPacket(ietf_quic,
                                                           supported_versions);
}

OwningSerializedPacketPointer
QuicPacketGenerator::SerializeConnectivityProbingPacket() {
  return packet_creator_.SerializeConnectivityProbingPacket();
}

OwningSerializedPacketPointer
QuicPacketGenerator::SerializePathChallengeConnectivityProbingPacket(
    QuicPathFrameBuffer* payload) {
  return packet_creator_.SerializePathChallengeConnectivityProbingPacket(
      payload);
}

OwningSerializedPacketPointer
QuicPacketGenerator::SerializePathResponseConnectivityProbingPacket(
    const QuicDeque<QuicPathFrameBuffer>& payloads,
    const bool is_padded) {
  return packet_creator_.SerializePathResponseConnectivityProbingPacket(
      payloads, is_padded);
}

void QuicPacketGenerator::ReserializeAllFrames(
    const QuicPendingRetransmission& retransmission,
    char* buffer,
    size_t buffer_len) {
  packet_creator_.ReserializeAllFrames(retransmission, buffer, buffer_len);
}

void QuicPacketGenerator::UpdatePacketNumberLength(
    QuicPacketNumber least_packet_awaited_by_peer,
    QuicPacketCount max_packets_in_flight) {
  return packet_creator_.UpdatePacketNumberLength(least_packet_awaited_by_peer,
                                                  max_packets_in_flight);
}

void QuicPacketGenerator::SetConnectionIdLength(uint32_t length) {
  if (length == 0) {
    packet_creator_.SetConnectionIdLength(PACKET_0BYTE_CONNECTION_ID);
  } else {
    packet_creator_.SetConnectionIdLength(PACKET_8BYTE_CONNECTION_ID);
  }
}

void QuicPacketGenerator::set_encryption_level(EncryptionLevel level) {
  packet_creator_.set_encryption_level(level);
}

void QuicPacketGenerator::SetEncrypter(
    EncryptionLevel level,
    std::unique_ptr<QuicEncrypter> encrypter) {
  packet_creator_.SetEncrypter(level, std::move(encrypter));
}

void QuicPacketGenerator::AddRandomPadding() {
  packet_creator_.AddPendingPadding(
      random_generator_->RandUint64() % kMaxNumRandomPaddingBytes + 1);
}

void QuicPacketGenerator::SendRemainingPendingPadding() {
  while (packet_creator_.pending_padding_bytes() > 0 && !HasQueuedFrames() &&
         CanSendWithNextPendingFrameAddition()) {
    packet_creator_.Flush();
  }
}

bool QuicPacketGenerator::HasRetransmittableFrames() const {
  return !queued_control_frames_.empty() ||
         packet_creator_.HasPendingRetransmittableFrames();
}

bool QuicPacketGenerator::HasPendingStreamFramesOfStream(
    QuicStreamId id) const {
  return packet_creator_.HasPendingStreamFramesOfStream(id);
}

void QuicPacketGenerator::SetTransmissionType(TransmissionType type) {
  packet_creator_.SetTransmissionType(type);
}

void QuicPacketGenerator::SetLongHeaderType(QuicLongHeaderType type) {
  packet_creator_.SetLongHeaderType(type);
}

void QuicPacketGenerator::SetCanSetTransmissionType(
    bool can_set_transmission_type) {
  packet_creator_.set_can_set_transmission_type(can_set_transmission_type);
}

MessageStatus QuicPacketGenerator::AddMessageFrame(QuicMessageId message_id,
                                                   QuicStringPiece message) {
  QUIC_BUG_IF(!flusher_attached_) << "Packet flusher is not attached when "
                                     "generator tries to add message frame.";
  if (message.length() > GetLargestMessagePayload()) {
    return MESSAGE_STATUS_TOO_LARGE;
  }
  SendQueuedFrames(/*flush=*/false);
  if (!packet_creator_.HasRoomForMessageFrame(message.length())) {
    packet_creator_.Flush();
  }
  QuicMessageFrame* frame = new QuicMessageFrame(message_id, message);
  const bool success = packet_creator_.AddSavedFrame(QuicFrame(frame));
  if (!success) {
    QUIC_BUG << "Failed to send message " << message_id;
    delete frame;
    return MESSAGE_STATUS_INTERNAL_ERROR;
  }
  return MESSAGE_STATUS_SUCCESS;
}

QuicPacketLength QuicPacketGenerator::GetLargestMessagePayload() const {
  return packet_creator_.GetLargestMessagePayload();
}

}  // namespace quic
