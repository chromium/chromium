// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A toy server, which listens on a specified address for QUIC traffic and
// handles incoming responses.
//
// Note that this server is intended to verify correctness of the client and is
// in no way expected to be performant.
#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_EPOLL_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_EPOLL_IMPL_H_

#include "net/third_party/quiche/src/quiche/epoll_server/simple_epoll_server.h"

namespace quiche {

using QuicheEpollServerImpl = epoll_server::SimpleEpollServer;
using QuicheEpollEventImpl = epoll_server::EpollEvent;
using QuicheEpollAlarmBaseImpl = epoll_server::EpollAlarm;
using QuicheEpollCallbackInterfaceImpl = epoll_server::EpollCallbackInterface;

}  // namespace quiche

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_EPOLL_IMPL_H_
