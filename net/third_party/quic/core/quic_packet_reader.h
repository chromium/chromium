// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class to read incoming QUIC packets from the UDP socket.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_PACKET_READER_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_PACKET_READER_H_

#include <netinet/in.h>
// Include here to guarantee this header gets included (for MSG_WAITFORONE)
// regardless of how the below transitive header include set may change.
#include <sys/socket.h>

#include "base/macros.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_process_packet_interface.h"
#include "net/third_party/quic/platform/api/quic_clock.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quic/platform/impl/quic_socket_utils.h"

#define MMSG_MORE 0

namespace quic {

#if MMSG_MORE
// Read in larger batches to minimize recvmmsg overhead.
const int kNumPacketsPerReadMmsgCall = 16;
#endif

class QuicPacketReader {
 public:
  QuicPacketReader();
  QuicPacketReader(const QuicPacketReader&) = delete;
  QuicPacketReader& operator=(const QuicPacketReader&) = delete;

  virtual ~QuicPacketReader();

  // Reads a number of packets from the given fd, and then passes them off to
  // the PacketProcessInterface.  Returns true if there may be additional
  // packets available on the socket.
  // Populates |packets_dropped| if it is non-null and the socket is configured
  // to track dropped packets and some packets are read.
  // If the socket has timestamping enabled, the per packet timestamps will be
  // passed to the processor. Otherwise, |clock| will be used.
  virtual bool ReadAndDispatchPackets(int fd,
                                      int port,
                                      const QuicClock& clock,
                                      ProcessPacketInterface* processor,
                                      QuicPacketCount* packets_dropped);

 private:
  // Initialize the internal state of the reader.
  void Initialize();

  // Reads and dispatches many packets using recvmmsg.
  bool ReadAndDispatchManyPackets(int fd,
                                  int port,
                                  const QuicClock& clock,
                                  ProcessPacketInterface* processor,
                                  QuicPacketCount* packets_dropped);

  // Reads and dispatches a single packet using recvmsg.
  static bool ReadAndDispatchSinglePacket(int fd,
                                          int port,
                                          const QuicClock& clock,
                                          ProcessPacketInterface* processor,
                                          QuicPacketCount* packets_dropped);

#if MMSG_MORE
  // Storage only used when recvmmsg is available.
  // TODO(danzh): change it to be a pointer to avoid the allocation on the stack
  // from exceeding maximum allowed frame size.
  // packets_ and mmsg_hdr_ are used to supply cbuf and buf to the recvmmsg
  // call.
  struct PacketData {
    iovec iov;
    // raw_address is used for address information provided by the recvmmsg
    // call on the packets.
    struct sockaddr_storage raw_address;
    // cbuf is used for ancillary data from the kernel on recvmmsg.
    char cbuf[kCmsgSpaceForReadPacket];
    // buf is used for the data read from the kernel on recvmmsg.
    char buf[kMaxV4PacketSize];
  };
  PacketData packets_[kNumPacketsPerReadMmsgCall];
  mmsghdr mmsg_hdr_[kNumPacketsPerReadMmsgCall];
#endif
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_PACKET_READER_H_
