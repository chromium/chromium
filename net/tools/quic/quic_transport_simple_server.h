// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_QUIC_TRANSPORT_SIMPLE_SERVER_H_
#define NET_TOOLS_QUIC_QUIC_TRANSPORT_SIMPLE_SERVER_H_

#include "base/memory/weak_ptr.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/platform/impl/quic_chromium_clock.h"
#include "net/socket/udp_server_socket.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_crypto_server_config.h"
#include "net/third_party/quiche/src/quic/core/quic_config.h"
#include "net/third_party/quiche/src/quic/core/quic_version_manager.h"
#include "net/third_party/quiche/src/quic/tools/quic_transport_simple_server_dispatcher.h"
#include "net/third_party/quiche/src/quic/tools/quic_transport_simple_server_session.h"
#include "url/origin.h"

namespace net {

// Server for QuicTransportSimpleSession.  This class is responsible for
// creating a UDP server socket, listening on it and passing the packets
// received to the dispatcher.
class QuicTransportSimpleServer {
 public:
  QuicTransportSimpleServer(int port,
                            quic::QuicTransportSimpleServerSession::Mode mode,
                            std::vector<url::Origin> accepted_origins);
  ~QuicTransportSimpleServer();

  int Run();

 private:
  // Schedules a ReadPackets() call on the next iteration of the event loop.
  void ScheduleReadPackets();
  // Reads a fixed number of packets and then reschedules itself.
  void ReadPackets();
  // Called when an asynchronous read from the socket is complete.
  void OnReadComplete(int result);
  // Passes the most recently read packet into the dispatcher.
  void ProcessReadPacket(int result);

  const int port_;

  quic::QuicVersionManager version_manager_;
  quic::QuicChromiumClock* clock_;  // Not owned.
  quic::QuicConfig config_;
  quic::QuicCryptoServerConfig crypto_config_;

  quic::QuicTransportSimpleServerDispatcher dispatcher_;
  std::unique_ptr<UDPServerSocket> socket_;
  IPEndPoint server_address_;

  // Results of the potentially asynchronous read operation.
  scoped_refptr<IOBufferWithSize> read_buffer_;
  IPEndPoint client_address_;

  base::WeakPtrFactory<QuicTransportSimpleServer> weak_factory_{this};
};

}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_TRANSPORT_SIMPLE_SERVER_H_
