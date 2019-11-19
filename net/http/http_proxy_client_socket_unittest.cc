// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_proxy_client_socket.h"

#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_server.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(HttpProxyClientSocketTest, Tag) {
  StaticSocketDataProvider data;
  MockTaggingStreamSocket* tagging_sock =
      new MockTaggingStreamSocket(std::make_unique<MockTCPClientSocket>(
          AddressList(), nullptr /* net_log */, &data));

  // |socket| takes ownership of |tagging_sock|, but the test keeps a non-owning
  // pointer to it.
  HttpProxyClientSocket socket(std::unique_ptr<StreamSocket>(tagging_sock), "",
                               HostPortPair(), ProxyServer(), nullptr, false,
                               false, NextProto(), nullptr,
                               TRAFFIC_ANNOTATION_FOR_TESTS);

  EXPECT_EQ(tagging_sock->tag(), SocketTag());
#if defined(OS_ANDROID)
  SocketTag tag(0x12345678, 0x87654321);
  socket.ApplySocketTag(tag);
  EXPECT_EQ(tagging_sock->tag(), tag);
#endif  // OS_ANDROID
}

}  // namespace

}  // namespace net
