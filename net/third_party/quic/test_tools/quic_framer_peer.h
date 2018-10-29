// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_TEST_TOOLS_QUIC_FRAMER_PEER_H_
#define NET_THIRD_PARTY_QUIC_TEST_TOOLS_QUIC_FRAMER_PEER_H_

#include "base/macros.h"
#include "net/third_party/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quic/core/quic_framer.h"
#include "net/third_party/quic/core/quic_packets.h"

namespace quic {

namespace test {

class QuicFramerPeer {
 public:
  QuicFramerPeer() = delete;

  static QuicPacketNumber CalculatePacketNumberFromWire(
      QuicFramer* framer,
      QuicPacketNumberLength packet_number_length,
      QuicPacketNumber last_packet_number,
      QuicPacketNumber packet_number);
  static void SetLastSerializedConnectionId(QuicFramer* framer,
                                            QuicConnectionId connection_id);
  static void SetLargestPacketNumber(QuicFramer* framer,
                                     QuicPacketNumber packet_number);
  static void SetPerspective(QuicFramer* framer, Perspective perspective);

  // SwapCrypters exchanges the state of the crypters of |framer1| with
  // |framer2|.
  static void SwapCrypters(QuicFramer* framer1, QuicFramer* framer2);

  static QuicEncrypter* GetEncrypter(QuicFramer* framer, EncryptionLevel level);

  static void SetLastPacketIsIetfQuic(QuicFramer* framer,
                                      bool last_packet_is_ietf_quic);

  // IETF defined frame append/process methods.
  static bool ProcessIetfStreamFrame(QuicFramer* framer,
                                     QuicDataReader* reader,
                                     uint8_t frame_type,
                                     QuicStreamFrame* frame);
  static bool AppendIetfStreamFrame(QuicFramer* framer,
                                    const QuicStreamFrame& frame,
                                    bool last_frame_in_packet,
                                    QuicDataWriter* writer);
  static bool ProcessCryptoFrame(QuicFramer* framer,
                                 QuicDataReader* reader,
                                 QuicCryptoFrame* frame);
  static bool AppendCryptoFrame(QuicFramer* framer,
                                const QuicCryptoFrame& frame,
                                QuicDataWriter* writer);

  static bool AppendIetfConnectionCloseFrame(
      QuicFramer* framer,
      const QuicConnectionCloseFrame& frame,
      QuicDataWriter* writer);
  static bool AppendApplicationCloseFrame(
      QuicFramer* framer,
      const QuicApplicationCloseFrame& frame,
      QuicDataWriter* writer);
  static bool ProcessIetfConnectionCloseFrame(QuicFramer* framer,
                                              QuicDataReader* reader,
                                              QuicConnectionCloseFrame* frame);
  static bool ProcessApplicationCloseFrame(QuicFramer* framer,
                                           QuicDataReader* reader,
                                           QuicApplicationCloseFrame* frame);
  static bool ProcessIetfAckFrame(QuicFramer* framer,
                                  QuicDataReader* reader,
                                  uint64_t frame_type,
                                  QuicAckFrame* ack_frame);
  static bool AppendIetfAckFrameAndTypeByte(QuicFramer* framer,
                                            const QuicAckFrame& frame,
                                            QuicDataWriter* writer);
  static size_t GetIetfAckFrameSize(QuicFramer* framer,
                                    const QuicAckFrame& frame);
  static bool AppendIetfResetStreamFrame(QuicFramer* framer,
                                         const QuicRstStreamFrame& frame,
                                         QuicDataWriter* writer);
  static bool ProcessIetfResetStreamFrame(QuicFramer* framer,
                                          QuicDataReader* reader,
                                          QuicRstStreamFrame* frame);

  static bool ProcessPathChallengeFrame(QuicFramer* framer,
                                        QuicDataReader* reader,
                                        QuicPathChallengeFrame* frame);
  static bool ProcessPathResponseFrame(QuicFramer* framer,
                                       QuicDataReader* reader,
                                       QuicPathResponseFrame* frame);

  static bool AppendPathChallengeFrame(QuicFramer* framer,
                                       const QuicPathChallengeFrame& frame,
                                       QuicDataWriter* writer);
  static bool AppendPathResponseFrame(QuicFramer* framer,
                                      const QuicPathResponseFrame& frame,
                                      QuicDataWriter* writer);

  static bool ProcessStopSendingFrame(QuicFramer* framer,
                                      QuicDataReader* reader,
                                      QuicStopSendingFrame* stop_sending_frame);
  static bool AppendStopSendingFrame(
      QuicFramer* framer,
      const QuicStopSendingFrame& stop_sending_frame,
      QuicDataWriter* writer);

  // Append/consume IETF-Format MAX_DATA and MAX_STREAM_DATA frames
  static bool AppendMaxDataFrame(QuicFramer* framer,
                                 const QuicWindowUpdateFrame& frame,
                                 QuicDataWriter* writer);
  static bool AppendMaxStreamDataFrame(QuicFramer* framer,
                                       const QuicWindowUpdateFrame& frame,
                                       QuicDataWriter* writer);
  static bool ProcessMaxDataFrame(QuicFramer* framer,
                                  QuicDataReader* reader,
                                  QuicWindowUpdateFrame* frame);
  static bool ProcessMaxStreamDataFrame(QuicFramer* framer,
                                        QuicDataReader* reader,
                                        QuicWindowUpdateFrame* frame);
  static bool AppendMaxStreamIdFrame(QuicFramer* framer,
                                     const QuicMaxStreamIdFrame& frame,
                                     QuicDataWriter* writer);
  static bool ProcessMaxStreamIdFrame(QuicFramer* framer,
                                      QuicDataReader* reader,
                                      QuicMaxStreamIdFrame* frame);
  static bool AppendIetfBlockedFrame(QuicFramer* framer,
                                     const QuicBlockedFrame& frame,
                                     QuicDataWriter* writer);
  static bool ProcessIetfBlockedFrame(QuicFramer* framer,
                                      QuicDataReader* reader,
                                      QuicBlockedFrame* frame);

  static bool AppendStreamBlockedFrame(QuicFramer* framer,
                                       const QuicBlockedFrame& frame,
                                       QuicDataWriter* writer);
  static bool ProcessStreamBlockedFrame(QuicFramer* framer,
                                        QuicDataReader* reader,
                                        QuicBlockedFrame* frame);

  static bool AppendStreamIdBlockedFrame(QuicFramer* framer,
                                         const QuicStreamIdBlockedFrame& frame,
                                         QuicDataWriter* writer);
  static bool ProcessStreamIdBlockedFrame(QuicFramer* framer,
                                          QuicDataReader* reader,
                                          QuicStreamIdBlockedFrame* frame);

  static bool AppendNewConnectionIdFrame(QuicFramer* framer,
                                         const QuicNewConnectionIdFrame& frame,
                                         QuicDataWriter* writer);
  static bool ProcessNewConnectionIdFrame(QuicFramer* framer,
                                          QuicDataReader* reader,
                                          QuicNewConnectionIdFrame* frame);
  static bool AppendRetireConnectionIdFrame(
      QuicFramer* framer,
      const QuicRetireConnectionIdFrame& frame,
      QuicDataWriter* writer);
  static bool ProcessRetireConnectionIdFrame(
      QuicFramer* framer,
      QuicDataReader* reader,
      QuicRetireConnectionIdFrame* frame);
  static size_t ComputeFrameLength(QuicFramer* framer,
                                   const QuicFrame& frame,
                                   bool last_frame_in_packet,
                                   QuicPacketNumberLength packet_number_length);
};

}  // namespace test

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_TEST_TOOLS_QUIC_FRAMER_PEER_H_
