// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_STREAM_PACKET_PROCESSOR_H_
#define REMOTING_PROTOCOL_STREAM_PACKET_PROCESSOR_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"

namespace net {
class IOBufferWithSize;
}  // namespace net

namespace rtc {
struct PacketTimeUpdateParams;
}  // namespace rtc

namespace remoting::protocol {

// Helper class to process packets from and to the StreamPacketSocket.
class StreamPacketProcessor {
 public:
  virtual ~StreamPacketProcessor() = default;

  // Packs data into packet to be sent over the packet socket. Returns nullptr
  // if the data is malformed.
  virtual scoped_refptr<net::IOBufferWithSize> Pack(const uint8_t* data,
                                                    size_t data_size) const = 0;

  // Unpacks a packet from the packet socket's buffer. |bytes_consumed| should
  // be updated with number of bytes it consumed from |data|. If the packet
  // can't be unpacked (generally because the packet is not fully received),
  // nullptr will be returned and |bytes_consumed| will be set to 0.
  virtual scoped_refptr<net::IOBufferWithSize> Unpack(
      const uint8_t* data,
      size_t data_size,
      size_t* bytes_consumed) const = 0;

  // Applies WebRTC packet options on a packet that has already been packed.
  virtual void ApplyPacketOptions(
      uint8_t* data,
      size_t data_size,
      const rtc::PacketTimeUpdateParams& packet_time_params) const = 0;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_STREAM_PACKET_PROCESSOR_H_
