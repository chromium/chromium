// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_client_message_loop_network_helper.h"

#include <utility>

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/address_utils.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/socket/udp_client_socket.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_random.h"
#include "net/third_party/quiche/src/quic/core/http/spdy_utils.h"
#include "net/third_party/quiche/src/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/core/quic_server_id.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quiche/src/spdy/core/spdy_header_block.h"

using std::string;

namespace net {

QuicClientMessageLooplNetworkHelper::QuicClientMessageLooplNetworkHelper(
    quic::QuicChromiumClock* clock,
    quic::QuicClientBase* client)
    : packet_reader_started_(false), clock_(clock), client_(client) {}

QuicClientMessageLooplNetworkHelper::~QuicClientMessageLooplNetworkHelper() =
    default;

bool QuicClientMessageLooplNetworkHelper::CreateUDPSocketAndBind(
    quic::QuicSocketAddress server_address,
    quic::QuicIpAddress bind_to_address,
    int bind_to_port) {
  auto socket = std::make_unique<UDPClientSocket>(DatagramSocket::DEFAULT_BIND,
                                                  nullptr, NetLogSource());

  if (bind_to_address.IsInitialized()) {
    client_address_ =
        quic::QuicSocketAddress(bind_to_address, client_->local_port());
  } else if (server_address.host().address_family() ==
             quic::IpAddressFamily::IP_V4) {
    client_address_ =
        quic::QuicSocketAddress(quic::QuicIpAddress::Any4(), bind_to_port);
  } else {
    client_address_ =
        quic::QuicSocketAddress(quic::QuicIpAddress::Any6(), bind_to_port);
  }

  int rc = socket->Connect(ToIPEndPoint(server_address));
  if (rc != OK) {
    LOG(ERROR) << "Connect failed: " << ErrorToShortString(rc);
    return false;
  }

  rc = socket->SetReceiveBufferSize(quic::kDefaultSocketReceiveBuffer);
  if (rc != OK) {
    LOG(ERROR) << "SetReceiveBufferSize() failed: " << ErrorToShortString(rc);
    return false;
  }

  rc = socket->SetSendBufferSize(quic::kDefaultSocketReceiveBuffer);
  if (rc != OK) {
    LOG(ERROR) << "SetSendBufferSize() failed: " << ErrorToShortString(rc);
    return false;
  }

  IPEndPoint address;
  rc = socket->GetLocalAddress(&address);
  if (rc != OK) {
    LOG(ERROR) << "GetLocalAddress failed: " << ErrorToShortString(rc);
    return false;
  }
  client_address_ = ToQuicSocketAddress(address);

  socket_.swap(socket);
  packet_reader_.reset(new QuicChromiumPacketReader(
      socket_.get(), clock_, this, kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      NetLogWithSource()));

  if (socket != nullptr) {
    socket->Close();
  }

  return true;
}

void QuicClientMessageLooplNetworkHelper::CleanUpAllUDPSockets() {
  client_->reset_writer();
  packet_reader_.reset();
  packet_reader_started_ = false;
}

void QuicClientMessageLooplNetworkHelper::StartPacketReaderIfNotStarted() {
  if (!packet_reader_started_) {
    packet_reader_->StartReading();
    packet_reader_started_ = true;
  }
}

void QuicClientMessageLooplNetworkHelper::RunEventLoop() {
  StartPacketReaderIfNotStarted();
  base::RunLoop().RunUntilIdle();
}

quic::QuicPacketWriter*
QuicClientMessageLooplNetworkHelper::CreateQuicPacketWriter() {
  // This is always called once per QuicSession before
  // StartPacketReaderIfNotStarted. However if the QuicClient is creating
  // multiple sessions it needs to restart the packet reader for the second one
  // so we set packet_reader_started_ to false to ensure that.
  packet_reader_started_ = false;

  return new QuicChromiumPacketWriter(
      socket_.get(), base::ThreadTaskRunnerHandle::Get().get());
}

void QuicClientMessageLooplNetworkHelper::OnReadError(
    int result,
    const DatagramClientSocket* socket) {
  LOG(ERROR) << "QuicSimpleClient read failed: " << ErrorToShortString(result);
  client_->Disconnect();
}

quic::QuicSocketAddress
QuicClientMessageLooplNetworkHelper::GetLatestClientAddress() const {
  return client_address_;
}

bool QuicClientMessageLooplNetworkHelper::OnPacket(
    const quic::QuicReceivedPacket& packet,
    const quic::QuicSocketAddress& local_address,
    const quic::QuicSocketAddress& peer_address) {
  client_->session()->connection()->ProcessUdpPacket(local_address,
                                                     peer_address, packet);
  if (!client_->session()->connection()->connected()) {
    return false;
  }

  return true;
}

}  // namespace net
