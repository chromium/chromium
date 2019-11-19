// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A toy client, which connects to a specified port and sends QUIC
// request to that endpoint.

#ifndef NET_TOOLS_QUIC_QUIC_SIMPLE_CLIENT_H_
#define NET_TOOLS_QUIC_QUIC_SIMPLE_CLIENT_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log.h"
#include "net/quic/platform/impl/quic_chromium_clock.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/third_party/quiche/src/quic/core/http/quic_spdy_stream.h"
#include "net/third_party/quiche/src/quic/core/quic_config.h"
#include "net/third_party/quiche/src/quic/tools/quic_spdy_client_base.h"
#include "net/tools/quic/quic_client_message_loop_network_helper.h"

namespace net {

class QuicChromiumAlarmFactory;
class QuicChromiumConnectionHelper;

namespace test {
class QuicClientPeer;
}  // namespace test

class QuicSimpleClient : public quic::QuicSpdyClientBase {
 public:
  // Create a quic client, which will have events managed by the message loop.
  QuicSimpleClient(quic::QuicSocketAddress server_address,
                   const quic::QuicServerId& server_id,
                   const quic::ParsedQuicVersionVector& supported_versions,
                   std::unique_ptr<quic::ProofVerifier> proof_verifier);

  ~QuicSimpleClient() override;

  std::unique_ptr<quic::QuicSession> CreateQuicClientSession(
      const quic::ParsedQuicVersionVector& supported_versions,
      quic::QuicConnection* connection) override;

 private:
  friend class net::test::QuicClientPeer;

  QuicChromiumAlarmFactory* CreateQuicAlarmFactory();
  QuicChromiumConnectionHelper* CreateQuicConnectionHelper();

  //  Used by |helper_| to time alarms.
  quic::QuicChromiumClock clock_;

  // Tracks if the client is initialized to connect.
  bool initialized_;

  base::WeakPtrFactory<QuicSimpleClient> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(QuicSimpleClient);
};

}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_SIMPLE_CLIENT_H_
