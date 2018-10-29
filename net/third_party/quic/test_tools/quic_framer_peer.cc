// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/test_tools/quic_framer_peer.h"

#include "net/third_party/quic/core/quic_framer.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/platform/api/quic_map_util.h"


namespace quic {
namespace test {

// static
QuicPacketNumber QuicFramerPeer::CalculatePacketNumberFromWire(
    QuicFramer* framer,
    QuicPacketNumberLength packet_number_length,
    QuicPacketNumber last_packet_number,
    QuicPacketNumber packet_number) {
  return framer->CalculatePacketNumberFromWire(
      packet_number_length, last_packet_number, packet_number);
}

// static
void QuicFramerPeer::SetLastSerializedConnectionId(
    QuicFramer* framer,
    QuicConnectionId connection_id) {
  framer->last_serialized_connection_id_ = connection_id;
}

// static
void QuicFramerPeer::SetLargestPacketNumber(QuicFramer* framer,
                                            QuicPacketNumber packet_number) {
  framer->largest_packet_number_ = packet_number;
}

// static
void QuicFramerPeer::SetPerspective(QuicFramer* framer,
                                    Perspective perspective) {
  framer->perspective_ = perspective;
}

// static
bool QuicFramerPeer::ProcessIetfStreamFrame(QuicFramer* framer,
                                            QuicDataReader* reader,
                                            uint8_t frame_type,
                                            QuicStreamFrame* frame) {
  return framer->ProcessIetfStreamFrame(reader, frame_type, frame);
}

// static
bool QuicFramerPeer::AppendIetfStreamFrame(QuicFramer* framer,
                                           const QuicStreamFrame& frame,
                                           bool last_frame_in_packet,
                                           QuicDataWriter* writer) {
  return framer->AppendIetfStreamFrame(frame, last_frame_in_packet, writer);
}

// static
bool QuicFramerPeer::ProcessCryptoFrame(QuicFramer* framer,
                                        QuicDataReader* reader,
                                        QuicCryptoFrame* frame) {
  return framer->ProcessCryptoFrame(reader, frame);
}

// static
bool QuicFramerPeer::AppendCryptoFrame(QuicFramer* framer,
                                       const QuicCryptoFrame& frame,
                                       QuicDataWriter* writer) {
  return framer->AppendCryptoFrame(frame, writer);
}

// static
bool QuicFramerPeer::ProcessIetfAckFrame(QuicFramer* framer,
                                         QuicDataReader* reader,
                                         uint64_t frame_type,
                                         QuicAckFrame* ack_frame) {
  return framer->ProcessIetfAckFrame(reader, frame_type, ack_frame);
}

// static
bool QuicFramerPeer::AppendIetfAckFrameAndTypeByte(QuicFramer* framer,
                                                   const QuicAckFrame& frame,
                                                   QuicDataWriter* writer) {
  return framer->AppendIetfAckFrameAndTypeByte(frame, writer);
}
// static
size_t QuicFramerPeer::GetIetfAckFrameSize(QuicFramer* framer,
                                           const QuicAckFrame& frame) {
  return framer->GetIetfAckFrameSize(frame);
}

// static
bool QuicFramerPeer::AppendIetfConnectionCloseFrame(
    QuicFramer* framer,
    const QuicConnectionCloseFrame& frame,
    QuicDataWriter* writer) {
  return framer->AppendIetfConnectionCloseFrame(frame, writer);
}

// static
bool QuicFramerPeer::AppendApplicationCloseFrame(
    QuicFramer* framer,
    const QuicApplicationCloseFrame& frame,
    QuicDataWriter* writer) {
  return framer->AppendApplicationCloseFrame(frame, writer);
}

// static
bool QuicFramerPeer::ProcessIetfConnectionCloseFrame(
    QuicFramer* framer,
    QuicDataReader* reader,
    QuicConnectionCloseFrame* frame) {
  return framer->ProcessIetfConnectionCloseFrame(reader, frame);
}

// static
bool QuicFramerPeer::ProcessApplicationCloseFrame(
    QuicFramer* framer,
    QuicDataReader* reader,
    QuicApplicationCloseFrame* frame) {
  return framer->ProcessApplicationCloseFrame(reader, frame);
}

// static
bool QuicFramerPeer::ProcessPathChallengeFrame(QuicFramer* framer,
                                               QuicDataReader* reader,
                                               QuicPathChallengeFrame* frame) {
  return framer->ProcessPathChallengeFrame(reader, frame);
}

// static
bool QuicFramerPeer::ProcessPathResponseFrame(QuicFramer* framer,
                                              QuicDataReader* reader,
                                              QuicPathResponseFrame* frame) {
  return framer->ProcessPathResponseFrame(reader, frame);
}

// static
bool QuicFramerPeer::AppendPathChallengeFrame(
    QuicFramer* framer,
    const QuicPathChallengeFrame& frame,
    QuicDataWriter* writer) {
  return framer->AppendPathChallengeFrame(frame, writer);
}

// static
bool QuicFramerPeer::AppendPathResponseFrame(QuicFramer* framer,
                                             const QuicPathResponseFrame& frame,
                                             QuicDataWriter* writer) {
  return framer->AppendPathResponseFrame(frame, writer);
}

// static
bool QuicFramerPeer::AppendIetfResetStreamFrame(QuicFramer* framer,
                                                const QuicRstStreamFrame& frame,
                                                QuicDataWriter* writer) {
  return framer->AppendIetfResetStreamFrame(frame, writer);
}

// static
bool QuicFramerPeer::ProcessIetfResetStreamFrame(QuicFramer* framer,
                                                 QuicDataReader* reader,
                                                 QuicRstStreamFrame* frame) {
  return framer->ProcessIetfResetStreamFrame(reader, frame);
}

// static
bool QuicFramerPeer::ProcessStopSendingFrame(
    QuicFramer* framer,
    QuicDataReader* reader,
    QuicStopSendingFrame* stop_sending_frame) {
  return framer->ProcessStopSendingFrame(reader, stop_sending_frame);
}

// static
bool QuicFramerPeer::AppendStopSendingFrame(
    QuicFramer* framer,
    const QuicStopSendingFrame& stop_sending_frame,
    QuicDataWriter* writer) {
  return framer->AppendStopSendingFrame(stop_sending_frame, writer);
}

// static
bool QuicFramerPeer::AppendMaxDataFrame(QuicFramer* framer,
                                        const QuicWindowUpdateFrame& frame,
                                        QuicDataWriter* writer) {
  return framer->AppendMaxDataFrame(frame, writer);
}

// static
bool QuicFramerPeer::AppendMaxStreamDataFrame(
    QuicFramer* framer,
    const QuicWindowUpdateFrame& frame,
    QuicDataWriter* writer) {
  return framer->AppendMaxStreamDataFrame(frame, writer);
}

// static
bool QuicFramerPeer::ProcessMaxDataFrame(QuicFramer* framer,
                                         QuicDataReader* reader,
                                         QuicWindowUpdateFrame* frame) {
  return framer->ProcessMaxDataFrame(reader, frame);
}

// static
bool QuicFramerPeer::ProcessMaxStreamDataFrame(QuicFramer* framer,
                                               QuicDataReader* reader,
                                               QuicWindowUpdateFrame* frame) {
  return framer->ProcessMaxStreamDataFrame(reader, frame);
}

// static
bool QuicFramerPeer::AppendMaxStreamIdFrame(QuicFramer* framer,
                                            const QuicMaxStreamIdFrame& frame,
                                            QuicDataWriter* writer) {
  return framer->AppendMaxStreamIdFrame(frame, writer);
}

// static
bool QuicFramerPeer::ProcessMaxStreamIdFrame(QuicFramer* framer,
                                             QuicDataReader* reader,
                                             QuicMaxStreamIdFrame* frame) {
  return framer->ProcessMaxStreamIdFrame(reader, frame);
}

// static
bool QuicFramerPeer::AppendIetfBlockedFrame(QuicFramer* framer,
                                            const QuicBlockedFrame& frame,
                                            QuicDataWriter* writer) {
  return framer->AppendIetfBlockedFrame(frame, writer);
}

// static
bool QuicFramerPeer::ProcessIetfBlockedFrame(QuicFramer* framer,
                                             QuicDataReader* reader,
                                             QuicBlockedFrame* frame) {
  return framer->ProcessIetfBlockedFrame(reader, frame);
}

// static
bool QuicFramerPeer::AppendStreamBlockedFrame(QuicFramer* framer,
                                              const QuicBlockedFrame& frame,
                                              QuicDataWriter* writer) {
  return framer->AppendStreamBlockedFrame(frame, writer);
}

// static
bool QuicFramerPeer::ProcessStreamBlockedFrame(QuicFramer* framer,
                                               QuicDataReader* reader,
                                               QuicBlockedFrame* frame) {
  return framer->ProcessStreamBlockedFrame(reader, frame);
}

// static
bool QuicFramerPeer::AppendStreamIdBlockedFrame(
    QuicFramer* framer,
    const QuicStreamIdBlockedFrame& frame,
    QuicDataWriter* writer) {
  return framer->AppendStreamIdBlockedFrame(frame, writer);
}

// static
bool QuicFramerPeer::ProcessStreamIdBlockedFrame(
    QuicFramer* framer,
    QuicDataReader* reader,
    QuicStreamIdBlockedFrame* frame) {
  return framer->ProcessStreamIdBlockedFrame(reader, frame);
}

// static
bool QuicFramerPeer::AppendNewConnectionIdFrame(
    QuicFramer* framer,
    const QuicNewConnectionIdFrame& frame,
    QuicDataWriter* writer) {
  return framer->AppendNewConnectionIdFrame(frame, writer);
}

// static
bool QuicFramerPeer::ProcessNewConnectionIdFrame(
    QuicFramer* framer,
    QuicDataReader* reader,
    QuicNewConnectionIdFrame* frame) {
  return framer->ProcessNewConnectionIdFrame(reader, frame);
}

// static
bool QuicFramerPeer::AppendRetireConnectionIdFrame(
    QuicFramer* framer,
    const QuicRetireConnectionIdFrame& frame,
    QuicDataWriter* writer) {
  return framer->AppendRetireConnectionIdFrame(frame, writer);
}

// static
bool QuicFramerPeer::ProcessRetireConnectionIdFrame(
    QuicFramer* framer,
    QuicDataReader* reader,
    QuicRetireConnectionIdFrame* frame) {
  return framer->ProcessRetireConnectionIdFrame(reader, frame);
}

// static
void QuicFramerPeer::SwapCrypters(QuicFramer* framer1, QuicFramer* framer2) {
  for (int i = ENCRYPTION_NONE; i < NUM_ENCRYPTION_LEVELS; i++) {
    framer1->encrypter_[i].swap(framer2->encrypter_[i]);
  }
  framer1->decrypter_.swap(framer2->decrypter_);
  framer1->alternative_decrypter_.swap(framer2->alternative_decrypter_);

  EncryptionLevel framer2_level = framer2->decrypter_level_;
  framer2->decrypter_level_ = framer1->decrypter_level_;
  framer1->decrypter_level_ = framer2_level;
  framer2_level = framer2->alternative_decrypter_level_;
  framer2->alternative_decrypter_level_ = framer1->alternative_decrypter_level_;
  framer1->alternative_decrypter_level_ = framer2_level;

  const bool framer2_latch = framer2->alternative_decrypter_latch_;
  framer2->alternative_decrypter_latch_ = framer1->alternative_decrypter_latch_;
  framer1->alternative_decrypter_latch_ = framer2_latch;
}

// static
QuicEncrypter* QuicFramerPeer::GetEncrypter(QuicFramer* framer,
                                            EncryptionLevel level) {
  return framer->encrypter_[level].get();
}

// static
void QuicFramerPeer::SetLastPacketIsIetfQuic(QuicFramer* framer,
                                             bool last_packet_is_ietf_quic) {
  framer->last_packet_is_ietf_quic_ = last_packet_is_ietf_quic;
}

// static
size_t QuicFramerPeer::ComputeFrameLength(
    QuicFramer* framer,
    const QuicFrame& frame,
    bool last_frame_in_packet,
    QuicPacketNumberLength packet_number_length) {
  return framer->ComputeFrameLength(frame, last_frame_in_packet,
                                    packet_number_length);
}

}  // namespace test
}  // namespace quic
