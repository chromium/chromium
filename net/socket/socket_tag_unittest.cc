// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_tag.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include <stdint.h>

#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/sockaddr_storage.h"
#include "net/socket/socket_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

// Test that SocketTag's comparison function work.
TEST(SocketTagTest, Compares) {
  SocketTag unset1;
  SocketTag unset2;

  EXPECT_TRUE(unset1 == unset2);
  EXPECT_FALSE(unset1 != unset2);
  EXPECT_FALSE(unset1 < unset2);

#if BUILDFLAG(IS_ANDROID)
  SocketTag s00(0, 0), s01(0, 1), s11(1, 1);

  EXPECT_FALSE(s00 == unset1);
  EXPECT_TRUE(s01 != unset2);
  EXPECT_FALSE(unset1 < s00);
  EXPECT_TRUE(s00 < unset2);

  EXPECT_FALSE(s00 == s01);
  EXPECT_FALSE(s01 == s11);
  EXPECT_FALSE(s00 == s11);
  EXPECT_TRUE(s00 < s01);
  EXPECT_TRUE(s01 < s11);
  EXPECT_TRUE(s00 < s11);
  EXPECT_FALSE(s01 < s00);
  EXPECT_FALSE(s11 < s01);
  EXPECT_FALSE(s11 < s00);
#endif
}

// On Android, where socket tagging is supported, verify that SocketTag::Apply
// works as expected.
#if BUILDFLAG(IS_ANDROID)
TEST(SocketTagTest, Apply) {
  if (!CanGetTaggedBytes()) {
    DVLOG(0) << "Skipping test - GetTaggedBytes unsupported.";
    return;
  }

  // Start test server.
  EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(base::FilePath());
  ASSERT_TRUE(test_server.Start());

  // Calculate sockaddr of test server.
  AddressList addr_list;
  ASSERT_TRUE(test_server.GetAddressList(&addr_list));
  SockaddrStorage addr;
  ASSERT_TRUE(addr_list[0].ToSockAddr(addr.addr, &addr.addr_len));

  // Create socket.
  int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_NE(s, -1);

  // Verify TCP connect packets are tagged and counted properly.
  int32_t tag_val1 = 0x12345678;
  uint64_t old_traffic = GetTaggedBytes(tag_val1);
  SocketTag tag1(SocketTag::UNSET_UID, tag_val1);
  tag1.Apply(s);
  ASSERT_EQ(connect(s, addr.addr, addr.addr_len), 0);
  EXPECT_GT(GetTaggedBytes(tag_val1), old_traffic);

  // Verify socket can be retagged with a new value and the current process's
  // UID.
  int32_t tag_val2 = 0x87654321;
  old_traffic = GetTaggedBytes(tag_val2);
  SocketTag tag2(getuid(), tag_val2);
  tag2.Apply(s);
  const char kRequest1[] = "GET / HTTP/1.0";
  ASSERT_EQ(send(s, kRequest1, strlen(kRequest1), 0),
            static_cast<int>(strlen(kRequest1)));
  EXPECT_GT(GetTaggedBytes(tag_val2), old_traffic);

  // Verify socket can be retagged with a new value and the current process's
  // UID.
  old_traffic = GetTaggedBytes(tag_val1);
  tag1.Apply(s);
  const char kRequest2[] = "\n\n";
  ASSERT_EQ(send(s, kRequest2, strlen(kRequest2), 0),
            static_cast<int>(strlen(kRequest2)));
  EXPECT_GT(GetTaggedBytes(tag_val1), old_traffic);

  ASSERT_EQ(close(s), 0);
}
#endif

}  // namespace net
