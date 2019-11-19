// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_QUIC_SIMPLE_SERVER_PACKET_WRITER_H_
#define NET_TOOLS_QUIC_QUIC_SIMPLE_SERVER_PACKET_WRITER_H_

#include <stddef.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/third_party/quiche/src/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quic/core/quic_packet_writer.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"

namespace quic {
class QuicDispatcher;
}  // namespace quic
namespace net {
class UDPServerSocket;
}  // namespace net
namespace quic {
struct WriteResult;
}  // namespace quic
namespace net {

// Chrome specific packet writer which uses a UDPServerSocket for writing
// data.
class QuicSimpleServerPacketWriter : public quic::QuicPacketWriter {
 public:
  typedef base::Callback<void(quic::WriteResult)> WriteCallback;

  QuicSimpleServerPacketWriter(UDPServerSocket* socket,
                               quic::QuicDispatcher* dispatcher);
  ~QuicSimpleServerPacketWriter() override;

  quic::WriteResult WritePacket(const char* buffer,
                                size_t buf_len,
                                const quic::QuicIpAddress& self_address,
                                const quic::QuicSocketAddress& peer_address,
                                quic::PerPacketOptions* options) override;

  void OnWriteComplete(int rv);

  // quic::QuicPacketWriter implementation:
  bool IsWriteBlocked() const override;
  void SetWritable() override;
  quic::QuicByteCount GetMaxPacketSize(
      const quic::QuicSocketAddress& peer_address) const override;
  bool SupportsReleaseTime() const override;
  bool IsBatchMode() const override;
  char* GetNextWriteLocation(
      const quic::QuicIpAddress& self_address,
      const quic::QuicSocketAddress& peer_address) override;
  quic::WriteResult Flush() override;

 private:
  UDPServerSocket* socket_;

  // To be notified after every successful asynchronous write.
  quic::QuicDispatcher* dispatcher_;

  // To call once the write completes.
  WriteCallback callback_;

  // Whether a write is currently in flight.
  bool write_blocked_;

  base::WeakPtrFactory<QuicSimpleServerPacketWriter> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(QuicSimpleServerPacketWriter);
};

}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_SIMPLE_SERVER_PACKET_WRITER_H_
