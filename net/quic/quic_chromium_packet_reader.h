// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef NET_QUIC_QUIC_CHROMIUM_PACKET_READER_H_
#define NET_QUIC_QUIC_CHROMIUM_PACKET_READER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/io_buffer.h"
#include "net/base/net_export.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/datagram_client_socket.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_time.h"

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
    virtual ~Visitor() = default;
    // Called when the read operation failed. The visitor returns
    // whether the reader should keep reading.
    virtual bool OnReadError(int result,
                             const DatagramClientSocket* socket) = 0;
    virtual bool OnPacket(const quic::QuicReceivedPacket& packet,
                          const quic::QuicSocketAddress& local_address,
                          const quic::QuicSocketAddress& peer_address) = 0;
  };

  // If |report_ecn| is true, then the reader will call GetLastTos() on the
  // socket after each read and report the ECN codepoint in the
  // QuicReceivedPacket.
  // TODO(crbug.com/332924003): When the relevant config flags are deprecated,
  // this argument can be removed.
  QuicChromiumPacketReader(std::unique_ptr<DatagramClientSocket> socket,
                           const quic::QuicClock* clock,
                           Visitor* visitor,
                           int yield_after_packets,
                           quic::QuicTime::Delta yield_after_duration,
                           bool report_ecn,
                           const NetLogWithSource& net_log);

  QuicChromiumPacketReader(const QuicChromiumPacketReader&) = delete;
  QuicChromiumPacketReader& operator=(const QuicChromiumPacketReader&) = delete;

  virtual ~QuicChromiumPacketReader();

  // Causes the QuicConnectionHelper to start reading from the socket
  // and passing the data along to the quic::QuicConnection.
  void StartReading();

  DatagramClientSocket* socket() { return socket_.get(); }

  void CloseSocket();

 private:
  // A completion callback invoked when a read completes.
  void OnReadComplete(int result);
  // Return true if reading should continue.
  bool ProcessReadResult(int result);

  std::unique_ptr<DatagramClientSocket> socket_;

  raw_ptr<Visitor> visitor_;
  bool read_pending_ = false;
  int num_packets_read_ = 0;
  raw_ptr<const quic::QuicClock> clock_;  // Not owned.
  int yield_after_packets_;
  quic::QuicTime::Delta yield_after_duration_;
  quic::QuicTime yield_after_;
  scoped_refptr<IOBufferWithSize> read_buffer_;
  NetLogWithSource net_log_;
  // Stores whether receiving ECN is in the feature list to avoid accessing
  // the feature list for every packet.
  bool report_ecn_;

  base::WeakPtrFactory<QuicChromiumPacketReader> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CHROMIUM_PACKET_READER_H_
