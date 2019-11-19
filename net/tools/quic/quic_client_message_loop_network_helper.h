// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A toy client, which connects to a specified port and sends QUIC
// request to that endpoint.

#ifndef NET_TOOLS_QUIC_QUIC_CLIENT_MESSAGE_LOOP_NETWORK_HELPER_H_
#define NET_TOOLS_QUIC_QUIC_CLIENT_MESSAGE_LOOP_NETWORK_HELPER_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_response_headers.h"
#include "net/quic/platform/impl/quic_chromium_clock.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/third_party/quiche/src/quic/core/http/quic_spdy_stream.h"
#include "net/third_party/quiche/src/quic/core/quic_config.h"
#include "net/third_party/quiche/src/quic/tools/quic_spdy_client_base.h"

namespace net {

class UDPClientSocket;

// An implementation of the quic::QuicClientBase::NetworkHelper based off
// the chromium epoll server.
class QuicClientMessageLooplNetworkHelper
    : public quic::QuicClientBase::NetworkHelper,
      public QuicChromiumPacketReader::Visitor {
 public:
  // Create a quic client, which will have events managed by an externally owned
  // EpollServer.
  QuicClientMessageLooplNetworkHelper(quic::QuicChromiumClock* clock,
                                      quic::QuicClientBase* client);

  ~QuicClientMessageLooplNetworkHelper() override;

  // QuicChromiumPacketReader::Visitor
  void OnReadError(int result, const DatagramClientSocket* socket) override;
  bool OnPacket(const quic::QuicReceivedPacket& packet,
                const quic::QuicSocketAddress& local_address,
                const quic::QuicSocketAddress& peer_address) override;

  // From NetworkHelper.
  void RunEventLoop() override;
  bool CreateUDPSocketAndBind(quic::QuicSocketAddress server_address,
                              quic::QuicIpAddress bind_to_address,
                              int bind_to_port) override;
  void CleanUpAllUDPSockets() override;
  quic::QuicSocketAddress GetLatestClientAddress() const override;
  quic::QuicPacketWriter* CreateQuicPacketWriter() override;

 private:
  void StartPacketReaderIfNotStarted();

  // Address of the client if the client is connected to the server.
  quic::QuicSocketAddress client_address_;

  // UDP socket connected to the server.
  std::unique_ptr<UDPClientSocket> socket_;

  std::unique_ptr<QuicChromiumPacketReader> packet_reader_;

  bool packet_reader_started_;

  quic::QuicChromiumClock* clock_;
  quic::QuicClientBase* client_;

  DISALLOW_COPY_AND_ASSIGN(QuicClientMessageLooplNetworkHelper);
};

}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_CLIENT_MESSAGE_LOOP_NETWORK_HELPER_H_
