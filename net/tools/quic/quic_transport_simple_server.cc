// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_transport_simple_server.h"

#include <stdlib.h>

#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/net_errors.h"
#include "net/quic/address_utils.h"
#include "net/quic/platform/impl/quic_chromium_clock.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/socket/udp_server_socket.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_default_proof_providers.h"
#include "net/third_party/quiche/src/quic/tools/quic_transport_simple_server_dispatcher.h"
#include "net/tools/quic/quic_simple_server_packet_writer.h"
#include "net/tools/quic/quic_simple_server_socket.h"

namespace net {
namespace {

using quic::CryptoHandshakeMessage;
using quic::ParsedQuicVersion;
using quic::PROTOCOL_TLS1_3;
using quic::QUIC_VERSION_99;
using quic::QuicChromiumClock;
using quic::QuicCryptoServerStream;
using quic::QuicSocketAddress;
using quic::QuicTransportSimpleServerSession;

constexpr char kSourceAddressTokenSecret[] = "test";
constexpr size_t kMaxReadsPerEvent = 32;
constexpr size_t kMaxNewConnectionsPerEvent = 32;
constexpr int kReadBufferSize = 2 * quic::kMaxIncomingPacketSize;

}  // namespace

class QuicTransportSimpleServerSessionHelper
    : public QuicCryptoServerStream::Helper {
 public:
  bool CanAcceptClientHello(const CryptoHandshakeMessage& /*message*/,
                            const QuicSocketAddress& /*client_address*/,
                            const QuicSocketAddress& /*peer_address*/,
                            const QuicSocketAddress& /*self_address*/,
                            std::string* /*error_details*/) const override {
    return true;
  }
};

QuicTransportSimpleServer::QuicTransportSimpleServer(
    int port,
    QuicTransportSimpleServerSession::Mode mode,
    std::vector<url::Origin> accepted_origins)
    : port_(port),
      version_manager_({ParsedQuicVersion{PROTOCOL_TLS1_3, QUIC_VERSION_99}}),
      clock_(QuicChromiumClock::GetInstance()),
      crypto_config_(kSourceAddressTokenSecret,
                     quic::QuicRandom::GetInstance(),
                     quic::CreateDefaultProofSource(),
                     quic::KeyExchangeSource::Default()),
      dispatcher_(&config_,
                  &crypto_config_,
                  &version_manager_,
                  std::make_unique<QuicChromiumConnectionHelper>(
                      clock_,
                      quic::QuicRandom::GetInstance()),
                  std::make_unique<QuicTransportSimpleServerSessionHelper>(),
                  std::make_unique<QuicChromiumAlarmFactory>(
                      base::ThreadTaskRunnerHandle::Get().get(),
                      clock_),
                  quic::kQuicDefaultConnectionIdLength,
                  mode,
                  accepted_origins),
      read_buffer_(base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize)) {}

QuicTransportSimpleServer::~QuicTransportSimpleServer() {}

int QuicTransportSimpleServer::Run() {
  socket_ = CreateQuicSimpleServerSocket(
      IPEndPoint{IPAddress::IPv6AllZeros(), port_}, &server_address_);
  if (socket_ == nullptr)
    return EXIT_FAILURE;

  dispatcher_.InitializeWithWriter(
      new QuicSimpleServerPacketWriter(socket_.get(), &dispatcher_));

  ScheduleReadPackets();
  base::RunLoop().Run();
  return EXIT_SUCCESS;
}

void QuicTransportSimpleServer::ScheduleReadPackets() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&QuicTransportSimpleServer::ReadPackets,
                                weak_factory_.GetWeakPtr()));
}

void QuicTransportSimpleServer::ReadPackets() {
  dispatcher_.ProcessBufferedChlos(kMaxNewConnectionsPerEvent);
  for (size_t i = 0; i < kMaxReadsPerEvent; i++) {
    int result = socket_->RecvFrom(
        read_buffer_.get(), read_buffer_->size(), &client_address_,
        base::BindOnce(&QuicTransportSimpleServer::OnReadComplete,
                       base::Unretained(this)));
    if (result == ERR_IO_PENDING)
      return;
    ProcessReadPacket(result);
  }
  ScheduleReadPackets();
}

void QuicTransportSimpleServer::OnReadComplete(int result) {
  ProcessReadPacket(result);
  ReadPackets();
}

void QuicTransportSimpleServer::ProcessReadPacket(int result) {
  if (result == 0)
    result = ERR_CONNECTION_CLOSED;
  if (result < 0) {
    LOG(ERROR) << "QuicTransportSimpleServer read failed: "
               << ErrorToString(result);
    dispatcher_.Shutdown();
    exit(EXIT_FAILURE);
    return;
  }

  quic::QuicReceivedPacket packet(read_buffer_->data(), /*length=*/result,
                                  clock_->Now(), /*owns_buffer=*/false);
  dispatcher_.ProcessPacket(ToQuicSocketAddress(server_address_),
                            ToQuicSocketAddress(client_address_), packet);
}

}  // namespace net
