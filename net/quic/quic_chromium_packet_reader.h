// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef NET_QUIC_QUIC_CHROMIUM_PACKET_READER_H_
#define NET_QUIC_QUIC_CHROMIUM_PACKET_READER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/base/io_buffer.h"
#include "net/base/net_export.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/datagram_client_socket.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/core/quic_time.h"

namespace quic {
class QuicClock;
}  // namespace quic
namespace net {

// If more than this many packets have been read or more than that many
// milliseconds have passed, QuicChromiumPacketReader::StartReading() yields by
// doing a QuicChromiumPacketReader::PostTask().
const int kQuicYieldAfterPacketsRead = 32;
const int kQuicYieldAfterDurationMilliseconds = 2;

class NET_EXPORT_PRIVATE QuicChromiumPacketReader {
 public:
  class NET_EXPORT_PRIVATE Visitor {
   public:
    virtual ~Visitor() {}
    virtual void OnReadError(int result,
                             const DatagramClientSocket* socket) = 0;
    virtual bool OnPacket(const quic::QuicReceivedPacket& packet,
                          const quic::QuicSocketAddress& local_address,
                          const quic::QuicSocketAddress& peer_address) = 0;
  };

  QuicChromiumPacketReader(DatagramClientSocket* socket,
                           const quic::QuicClock* clock,
                           Visitor* visitor,
                           int yield_after_packets,
                           quic::QuicTime::Delta yield_after_duration,
                           const NetLogWithSource& net_log);
  virtual ~QuicChromiumPacketReader();

  // Causes the QuicConnectionHelper to start reading from the socket
  // and passing the data along to the quic::QuicConnection.
  void StartReading();

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

 private:
  // A completion callback invoked when a read completes.
  void OnReadComplete(int result);
  // Return true if reading should continue.
  bool ProcessReadResult(int result);

  DatagramClientSocket* socket_;

  Visitor* visitor_;
  bool read_pending_;
  int num_packets_read_;
  const quic::QuicClock* clock_;  // Not owned.
  int yield_after_packets_;
  quic::QuicTime::Delta yield_after_duration_;
  quic::QuicTime yield_after_;
  scoped_refptr<IOBufferWithSize> read_buffer_;
  NetLogWithSource net_log_;

  base::WeakPtrFactory<QuicChromiumPacketReader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(QuicChromiumPacketReader);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CHROMIUM_PACKET_READER_H_
