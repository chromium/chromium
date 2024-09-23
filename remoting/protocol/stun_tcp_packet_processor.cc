// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/protocol/stun_tcp_packet_processor.h"

#include "base/containers/span.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "net/base/io_buffer.h"
#include "third_party/webrtc/media/base/rtp_utils.h"
#include "third_party/webrtc/rtc_base/time_utils.h"

namespace remoting::protocol {

namespace {

using PacketLength = uint16_t;
constexpr size_t kPacketHeaderSize = sizeof(PacketLength);
constexpr size_t kStunHeaderSize = 20;
constexpr size_t kTurnChannelDataHeaderSize = 4;
constexpr size_t kPacketLengthOffset = 2;

int GetExpectedStunPacketSize(const uint8_t* data_ptr,
                              size_t len,
                              size_t* pad_bytes) {
  // TODO(crbug.com/40284755): GetExpectedStunPacketSize() should receive a
  // span.
  auto data = UNSAFE_TODO(base::span(data_ptr, len));
  DCHECK_LE(kTurnChannelDataHeaderSize, data.size());

  // Get packet type (STUN or TURN).
  uint16_t msg_type = base::numerics::U16FromBigEndian(data.subspan<0u, 2u>());
  // Both stun and turn had length at offset 2.
  size_t packet_size =
      base::numerics::U16FromBigEndian(data.subspan<kPacketLengthOffset, 2u>());

  *pad_bytes = 0;
  // Add header length to packet length.
  if ((msg_type & 0xC000) == 0) {
    packet_size += kStunHeaderSize;
  } else {
    packet_size += kTurnChannelDataHeaderSize;
    // Calculate any padding if present.
    if (packet_size % 4) {
      *pad_bytes = 4 - packet_size % 4;
    }
  }
  return packet_size;
}

}  // namespace

StunTcpPacketProcessor::StunTcpPacketProcessor() = default;

StunTcpPacketProcessor::~StunTcpPacketProcessor() = default;

// static
StunTcpPacketProcessor* StunTcpPacketProcessor::GetInstance() {
  static base::NoDestructor<StunTcpPacketProcessor> instance;
  return instance.get();
}

scoped_refptr<net::IOBufferWithSize> StunTcpPacketProcessor::Pack(
    const uint8_t* data,
    size_t data_size) const {
  // Each packet is expected to have header (STUN/TURN ChannelData), where
  // header contains message type and and length of message.
  if (data_size < kPacketHeaderSize + kPacketLengthOffset) {
    NOTREACHED();
  }

  size_t pad_bytes;
  size_t expected_len = GetExpectedStunPacketSize(data, data_size, &pad_bytes);

  // Accepts only complete STUN/TURN packets.
  if (data_size != expected_len) {
    NOTREACHED();
  }

  // Add any pad bytes to the total size.
  size_t size = data_size + pad_bytes;

  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(size);
  memcpy(buffer->data(), data, data_size);

  if (pad_bytes) {
    char padding[4] = {0};
    DCHECK_LE(pad_bytes, 4u);
    memcpy(buffer->data() + data_size, padding, pad_bytes);
  }
  return buffer;
}

scoped_refptr<net::IOBufferWithSize> StunTcpPacketProcessor::Unpack(
    const uint8_t* data,
    size_t data_size,
    size_t* bytes_consumed) const {
  *bytes_consumed = 0;
  if (data_size < kPacketHeaderSize + kPacketLengthOffset) {
    return nullptr;
  }

  size_t pad_bytes;
  size_t packet_size = GetExpectedStunPacketSize(data, data_size, &pad_bytes);

  if (data_size < packet_size + pad_bytes) {
    return nullptr;
  }

  // We have a complete packet.
  const uint8_t* cur = data;
  *bytes_consumed = packet_size + pad_bytes;
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(packet_size);
  memcpy(buffer->data(), cur, packet_size);
  return buffer;
}

void StunTcpPacketProcessor::ApplyPacketOptions(
    uint8_t* data,
    size_t data_size,
    const rtc::PacketTimeUpdateParams& packet_time_params) const {
  cricket::ApplyPacketOptions(data, data_size, packet_time_params,
                              rtc::TimeMicros());
}

}  // namespace remoting::protocol
