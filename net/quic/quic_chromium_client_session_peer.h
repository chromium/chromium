// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CHROMIUM_CLIENT_SESSION_PEER_H_
#define NET_QUIC_QUIC_CHROMIUM_CLIENT_SESSION_PEER_H_

#include <stddef.h>

#include <string>

#include "net/quic/quic_chromium_client_session.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"

namespace net {

class QuicChromiumClientStream;

namespace test {

class QuicChromiumClientSessionPeer {
 public:
  QuicChromiumClientSessionPeer(const QuicChromiumClientSessionPeer&) = delete;
  QuicChromiumClientSessionPeer& operator=(
      const QuicChromiumClientSessionPeer&) = delete;

  static void SetHostname(QuicChromiumClientSession* session,
                          const std::string& hostname);

  static QuicChromiumClientStream* CreateOutgoingStream(
      QuicChromiumClientSession* session);

  static bool GetSessionGoingAway(QuicChromiumClientSession* session);

  static MigrationCause GetCurrentMigrationCause(
      QuicChromiumClientSession* session);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_QUIC_CHROMIUM_CLIENT_SESSION_PEER_H_
