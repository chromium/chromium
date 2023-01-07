// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_simple_server_session_helper.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_connection_id.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"

namespace net {

QuicSimpleServerSessionHelper::QuicSimpleServerSessionHelper(
    quic::QuicRandom* random) {}

QuicSimpleServerSessionHelper::~QuicSimpleServerSessionHelper() = default;

bool QuicSimpleServerSessionHelper::CanAcceptClientHello(
    const quic::CryptoHandshakeMessage& message,
    const quic::QuicSocketAddress& client_address,
    const quic::QuicSocketAddress& peer_address,
    const quic::QuicSocketAddress& self_address,
    std::string* error_details) const {
  return true;
}

}  // namespace net
