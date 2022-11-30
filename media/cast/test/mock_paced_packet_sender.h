// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_MOCK_PACED_PACKET_SENDER_H_
#define MEDIA_CAST_TEST_MOCK_PACED_PACKET_SENDER_H_

#include "media/cast/net/pacing/paced_sender.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

class MockPacedPacketSender : public PacedPacketSender {
 public:
  MockPacedPacketSender();
  ~MockPacedPacketSender() override;

  MOCK_METHOD1(SendPackets, bool(const SendPacketVector& packets));
  MOCK_METHOD2(ResendPackets,
               bool(const SendPacketVector& packets,
                    const DedupInfo& dedup_info));
  MOCK_METHOD2(SendRtcpPacket, bool(unsigned int ssrc, PacketRef packet));
  MOCK_METHOD1(CancelSendingPacket, void(const PacketKey& packet_key));
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_MOCK_PACED_PACKET_SENDER_H_
