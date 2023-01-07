// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_EPOLL_TEST_TOOLS_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_EPOLL_TEST_TOOLS_IMPL_H_

#include "net/third_party/quiche/src/quiche/epoll_server/fake_simple_epoll_server.h"

namespace quiche {

using QuicheFakeEpollServerImpl = epoll_server::test::FakeSimpleEpollServer;

}

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_EPOLL_TEST_TOOLS_IMPL_H_
