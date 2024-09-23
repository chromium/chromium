// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_simple_client.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/socket/udp_client_socket.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_random.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/spdy_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quiche/quic/tools/quic_simple_client_session.h"

using std::string;

namespace net {

QuicSimpleClient::QuicSimpleClient(
    quic::QuicSocketAddress server_address,
    const quic::QuicServerId& server_id,
    const quic::ParsedQuicVersionVector& supported_versions,
    const quic::QuicConfig& config,
    std::unique_ptr<quic::ProofVerifier> proof_verifier)
    : quic::QuicSpdyClientBase(
          server_id,
          supported_versions,
          config,
          CreateQuicConnectionHelper(),
          CreateQuicAlarmFactory(),
          std::make_unique<QuicClientMessageLooplNetworkHelper>(&clock_, this),
          std::move(proof_verifier),
          nullptr) {
  set_server_address(server_address);
}

QuicSimpleClient::~QuicSimpleClient() {
  if (connected()) {
    session()->connection()->CloseConnection(
        quic::QUIC_PEER_GOING_AWAY, "Shutting down",
        quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
  }
}

std::unique_ptr<quic::QuicSession> QuicSimpleClient::CreateQuicClientSession(
    const quic::ParsedQuicVersionVector& supported_versions,
    quic::QuicConnection* connection) {
  return std::make_unique<quic::QuicSimpleClientSession>(
      *config(), supported_versions, connection, network_helper(), server_id(),
      crypto_config(), drop_response_body(), /*enable_web_transport=*/false);
}

QuicChromiumConnectionHelper* QuicSimpleClient::CreateQuicConnectionHelper() {
  return new QuicChromiumConnectionHelper(&clock_,
                                          quic::QuicRandom::GetInstance());
}

QuicChromiumAlarmFactory* QuicSimpleClient::CreateQuicAlarmFactory() {
  return new QuicChromiumAlarmFactory(
      base::SingleThreadTaskRunner::GetCurrentDefault().get(), &clock_);
}

}  // namespace net
