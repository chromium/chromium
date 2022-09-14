// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_simple_server_socket.h"

#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_constants.h"

namespace net {

std::unique_ptr<UDPServerSocket> CreateQuicSimpleServerSocket(
    const IPEndPoint& address,
    IPEndPoint* server_address) {
  auto socket =
      std::make_unique<UDPServerSocket>(/*net_log=*/nullptr, NetLogSource());

  socket->AllowAddressReuse();

  int rc = socket->Listen(address);
  if (rc < 0) {
    LOG(ERROR) << "Listen() failed: " << ErrorToString(rc);
    return nullptr;
  }

  // These send and receive buffer sizes are sized for a single connection,
  // because the default usage of QuicSimpleServer is as a test server with
  // one or two clients.  Adjust higher for use with many clients.
  rc = socket->SetReceiveBufferSize(
      static_cast<int32_t>(quic::kDefaultSocketReceiveBuffer));
  if (rc < 0) {
    LOG(ERROR) << "SetReceiveBufferSize() failed: " << ErrorToString(rc);
    return nullptr;
  }

  rc = socket->SetSendBufferSize(20 * quic::kMaxOutgoingPacketSize);
  if (rc < 0) {
    LOG(ERROR) << "SetSendBufferSize() failed: " << ErrorToString(rc);
    return nullptr;
  }

  rc = socket->GetLocalAddress(server_address);
  if (rc < 0) {
    LOG(ERROR) << "GetLocalAddress() failed: " << ErrorToString(rc);
    return nullptr;
  }

  VLOG(1) << "Listening on " << server_address->ToString();
  return socket;
}

}  // namespace net
