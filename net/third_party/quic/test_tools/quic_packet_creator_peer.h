// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_TEST_TOOLS_QUIC_PACKET_CREATOR_PEER_H_
#define NET_THIRD_PARTY_QUIC_TEST_TOOLS_QUIC_PACKET_CREATOR_PEER_H_

#include "base/macros.h"
#include "net/third_party/quic/core/quic_packets.h"

namespace quic {
class QuicFramer;
class QuicPacketCreator;

namespace test {

class QuicPacketCreatorPeer {
 public:
  QuicPacketCreatorPeer() = delete;

  static bool SendVersionInPacket(QuicPacketCreator* creator);

  static void SetSendVersionInPacket(QuicPacketCreator* creator,
                                     bool send_version_in_packet);
  static void SetPacketNumberLength(
      QuicPacketCreator* creator,
      QuicPacketNumberLength packet_number_length);
  static QuicPacketNumberLength GetPacketNumberLength(
      QuicPacketCreator* creator);
  static void SetPacketNumber(QuicPacketCreator* creator, QuicPacketNumber s);
  static void FillPacketHeader(QuicPacketCreator* creator,
                               QuicPacketHeader* header);
  static void CreateStreamFrame(QuicPacketCreator* creator,
                                QuicStreamId id,
                                size_t write_length,
                                size_t iov_offset,
                                QuicStreamOffset offset,
                                bool fin,
                                QuicFrame* frame);
  static SerializedPacket SerializeAllFrames(QuicPacketCreator* creator,
                                             const QuicFrames& frames,
                                             char* buffer,
                                             size_t buffer_len);
  static OwningSerializedPacketPointer SerializeConnectivityProbingPacket(
      QuicPacketCreator* creator);
  static OwningSerializedPacketPointer
  SerializePathChallengeConnectivityProbingPacket(QuicPacketCreator* creator,
                                                  QuicPathFrameBuffer* payload);

  static EncryptionLevel GetEncryptionLevel(QuicPacketCreator* creator);
  static QuicFramer* framer(QuicPacketCreator* creator);
};

}  // namespace test

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_TEST_TOOLS_QUIC_PACKET_CREATOR_PEER_H_
