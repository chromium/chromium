// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_simple_server.h"

#include <string.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/quic/address_utils.h"
#include "net/socket/udp_server_socket.h"
#include "net/third_party/quiche/src/quic/core/crypto/crypto_handshake.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_random.h"
#include "net/third_party/quiche/src/quic/core/quic_crypto_stream.h"
#include "net/third_party/quiche/src/quic/core/quic_data_reader.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/tools/quic_simple_dispatcher.h"
#include "net/tools/quic/quic_simple_server_packet_writer.h"
#include "net/tools/quic/quic_simple_server_session_helper.h"
#include "net/tools/quic/quic_simple_server_socket.h"

namespace net {

namespace {

const char kSourceAddressTokenSecret[] = "secret";
const size_t kNumSessionsToCreatePerSocketEvent = 16;

// Allocate some extra space so we can send an error if the client goes over
// the limit.
const int kReadBufferSize = 2 * quic::kMaxIncomingPacketSize;

}  // namespace

QuicSimpleServer::QuicSimpleServer(
    std::unique_ptr<quic::ProofSource> proof_source,
    const quic::QuicConfig& config,
    const quic::QuicCryptoServerConfig::ConfigOptions& crypto_config_options,
    const quic::ParsedQuicVersionVector& supported_versions,
    quic::QuicSimpleServerBackend* quic_simple_server_backend)
    : version_manager_(supported_versions),
      helper_(
          new QuicChromiumConnectionHelper(&clock_,
                                           quic::QuicRandom::GetInstance())),
      alarm_factory_(new QuicChromiumAlarmFactory(
          base::ThreadTaskRunnerHandle::Get().get(),
          &clock_)),
      config_(config),
      crypto_config_options_(crypto_config_options),
      crypto_config_(kSourceAddressTokenSecret,
                     quic::QuicRandom::GetInstance(),
                     std::move(proof_source),
                     quic::KeyExchangeSource::Default()),
      read_pending_(false),
      synchronous_read_count_(0),
      read_buffer_(base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize)),
      quic_simple_server_backend_(quic_simple_server_backend) {
  DCHECK(quic_simple_server_backend);
  Initialize();
}

void QuicSimpleServer::Initialize() {
#if MMSG_MORE
  use_recvmmsg_ = true;
#endif

  // If an initial flow control window has not explicitly been set, then use a
  // sensible value for a server: 1 MB for session, 64 KB for each stream.
  const uint32_t kInitialSessionFlowControlWindow = 1 * 1024 * 1024;  // 1 MB
  const uint32_t kInitialStreamFlowControlWindow = 64 * 1024;         // 64 KB
  if (config_.GetInitialStreamFlowControlWindowToSend() ==
      quic::kMinimumFlowControlSendWindow) {
    config_.SetInitialStreamFlowControlWindowToSend(
        kInitialStreamFlowControlWindow);
  }
  if (config_.GetInitialSessionFlowControlWindowToSend() ==
      quic::kMinimumFlowControlSendWindow) {
    config_.SetInitialSessionFlowControlWindowToSend(
        kInitialSessionFlowControlWindow);
  }

  std::unique_ptr<quic::CryptoHandshakeMessage> scfg(
      crypto_config_.AddDefaultConfig(helper_->GetRandomGenerator(),
                                      helper_->GetClock(),
                                      crypto_config_options_));
}

QuicSimpleServer::~QuicSimpleServer() = default;

bool QuicSimpleServer::CreateUDPSocketAndListen(
    const quic::QuicSocketAddress& address) {
  return Listen(ToIPEndPoint(address));
}

void QuicSimpleServer::HandleEventsForever() {
  base::RunLoop().Run();
}

bool QuicSimpleServer::Listen(const IPEndPoint& address) {
  socket_ = CreateQuicSimpleServerSocket(address, &server_address_);
  if (socket_ == nullptr)
    return false;

  dispatcher_.reset(new quic::QuicSimpleDispatcher(
      &config_, &crypto_config_, &version_manager_,
      std::unique_ptr<quic::QuicConnectionHelperInterface>(helper_),
      std::unique_ptr<quic::QuicCryptoServerStream::Helper>(
          new QuicSimpleServerSessionHelper(quic::QuicRandom::GetInstance())),
      std::unique_ptr<quic::QuicAlarmFactory>(alarm_factory_),
      quic_simple_server_backend_, quic::kQuicDefaultConnectionIdLength));
  QuicSimpleServerPacketWriter* writer =
      new QuicSimpleServerPacketWriter(socket_.get(), dispatcher_.get());
  dispatcher_->InitializeWithWriter(writer);

  StartReading();

  return true;
}

void QuicSimpleServer::Shutdown() {
  // Before we shut down the epoll server, give all active sessions a chance to
  // notify clients that they're closing.
  dispatcher_->Shutdown();

  if (!socket_) {
    return;
  }
  socket_->Close();
  socket_.reset();
}

void QuicSimpleServer::StartReading() {
  if (synchronous_read_count_ == 0) {
    // Only process buffered packets once per message loop.
    dispatcher_->ProcessBufferedChlos(kNumSessionsToCreatePerSocketEvent);
  }

  if (read_pending_) {
    return;
  }
  read_pending_ = true;

  int result = socket_->RecvFrom(
      read_buffer_.get(), read_buffer_->size(), &client_address_,
      base::BindOnce(&QuicSimpleServer::OnReadComplete,
                     base::Unretained(this)));

  if (result == ERR_IO_PENDING) {
    synchronous_read_count_ = 0;
    if (dispatcher_->HasChlosBuffered()) {
      // No more packets to read, so yield before processing buffered packets.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&QuicSimpleServer::StartReading,
                                    weak_factory_.GetWeakPtr()));
    }
    return;
  }

  if (++synchronous_read_count_ > 32) {
    synchronous_read_count_ = 0;
    // Schedule the processing through the message loop to 1) prevent infinite
    // recursion and 2) avoid blocking the thread for too long.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&QuicSimpleServer::OnReadComplete,
                                  weak_factory_.GetWeakPtr(), result));
  } else {
    OnReadComplete(result);
  }
}

void QuicSimpleServer::OnReadComplete(int result) {
  read_pending_ = false;
  if (result == 0)
    result = ERR_CONNECTION_CLOSED;

  if (result < 0) {
    LOG(ERROR) << "QuicSimpleServer read failed: " << ErrorToString(result);
    Shutdown();
    return;
  }

  quic::QuicReceivedPacket packet(read_buffer_->data(), result,
                                  helper_->GetClock()->Now(), false);
  dispatcher_->ProcessPacket(ToQuicSocketAddress(server_address_),
                             ToQuicSocketAddress(client_address_), packet);

  StartReading();
}

}  // namespace net
