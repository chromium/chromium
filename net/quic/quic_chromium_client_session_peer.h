// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CHROMIUM_CLIENT_SESSION_PEER_H_
#define NET_QUIC_QUIC_CHROMIUM_CLIENT_SESSION_PEER_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"

namespace net {

class QuicChromiumClientSession;
class QuicChromiumClientStream;

namespace test {

class QuicChromiumClientSessionPeer {
 public:
  static void SetHostname(QuicChromiumClientSession* session,
                          const std::string& hostname);

  static uint64_t GetPushedBytesCount(QuicChromiumClientSession* session);

  static uint64_t GetPushedAndUnclaimedBytesCount(
      QuicChromiumClientSession* session);

  static QuicChromiumClientStream* CreateOutgoingStream(
      QuicChromiumClientSession* session);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicChromiumClientSessionPeer);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_QUIC_CHROMIUM_CLIENT_SESSION_PEER_H_
