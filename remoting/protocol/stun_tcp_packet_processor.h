// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_STUN_TCP_PACKET_PROCESSOR_H_
#define REMOTING_PROTOCOL_STUN_TCP_PACKET_PROCESSOR_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "remoting/protocol/stream_packet_processor.h"

namespace remoting::protocol {

// StreamPacketSocket implementation for data that has already been packed in
// STUN/TURN's TCP packet. It won't add any extra header to the data but will
// adjust for padding to meet the 4-byte alignment requirement per RFC 5766
// section 11.5.
class StunTcpPacketProcessor final : public StreamPacketProcessor {
 public:
  StunTcpPacketProcessor();
  ~StunTcpPacketProcessor() override;

  StunTcpPacketProcessor(const StunTcpPacketProcessor&) = delete;
  StunTcpPacketProcessor& operator=(const StunTcpPacketProcessor&) = delete;

  static StunTcpPacketProcessor* GetInstance();

  // StreamPacketProcessor implementations.
  scoped_refptr<net::IOBufferWithSize> Pack(const uint8_t* data,
                                            size_t data_size) const override;
  scoped_refptr<net::IOBufferWithSize> Unpack(
      const uint8_t* data,
      size_t data_size,
      size_t* bytes_consumed) const override;
  void ApplyPacketOptions(
      uint8_t* data,
      size_t data_size,
      const rtc::PacketTimeUpdateParams& packet_time_params) const override;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_STUN_TCP_PACKET_PROCESSOR_H_
