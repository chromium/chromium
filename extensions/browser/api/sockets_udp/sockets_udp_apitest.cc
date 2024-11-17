// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/api/socket/write_quota_checker.h"
#include "extensions/browser/api/sockets_udp/sockets_udp_api.h"
#include "extensions/browser/api/sockets_udp/test_udp_echo_server.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/shell/test/shell_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

const char kHostname[] = "www.foo.com";
const int kPort = 8888;

class SocketsUdpApiTest : public ShellApiTest {
 public:
  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();
    host_resolver()->AddRule(kHostname, "127.0.0.1");
  }
};

IN_PROC_BROWSER_TEST_F(SocketsUdpApiTest, SocketsUdpCreateGood) {
  scoped_refptr<api::SocketsUdpCreateFunction> socket_create_function(
      new api::SocketsUdpCreateFunction());
  scoped_refptr<const Extension> empty_extension =
      ExtensionBuilder("Test").Build();

  socket_create_function->set_extension(empty_extension.get());
  socket_create_function->set_has_callback(true);

  std::optional<base::Value> result(
      api_test_utils::RunFunctionAndReturnSingleResult(
          socket_create_function.get(), "[]", browser_context()));

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_dict());
  std::optional<int> socket_id = result->GetDict().FindInt("socketId");
  ASSERT_TRUE(socket_id);
  ASSERT_GT(*socket_id, 0);
}

IN_PROC_BROWSER_TEST_F(SocketsUdpApiTest, SocketsUdpExtension) {
  TestUdpEchoServer udp_echo_server;
  net::HostPortPair host_port_pair;
  ASSERT_TRUE(udp_echo_server.Start(
      browser_context()->GetDefaultStoragePartition()->GetNetworkContext(),
      &host_port_pair));

  int port = host_port_pair.port();
  ASSERT_GT(port, 0);

  // Test that sendTo() is properly resolving hostnames.
  host_port_pair.set_host(kHostname);

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser_context());

  ExtensionTestMessageListener listener("info_please",
                                        ReplyBehavior::kWillReply);

  ASSERT_TRUE(LoadApp("sockets_udp/api"));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(
      base::StringPrintf("udp:%s:%d", host_port_pair.host().c_str(), port));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// See crbug.com/321451.
IN_PROC_BROWSER_TEST_F(SocketsUdpApiTest, DISABLED_SocketsUdpMulticast) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser_context());
  ExtensionTestMessageListener listener("info_please",
                                        ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadApp("sockets_udp/api"));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(base::StringPrintf("multicast:%s:%d", kHostname, kPort));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(SocketsUdpApiTest, SocketsUdpSendWriteQuota) {
  WriteQuotaChecker* write_quota_checker =
      WriteQuotaChecker::Get(browser_context());
  constexpr size_t kBytesLimit = 1;
  WriteQuotaChecker::ScopedBytesLimitForTest scoped_quota(write_quota_checker,
                                                          kBytesLimit);

  TestUdpEchoServer udp_echo_server;
  net::HostPortPair host_port_pair;
  ASSERT_TRUE(udp_echo_server.Start(
      browser_context()->GetDefaultStoragePartition()->GetNetworkContext(),
      &host_port_pair));

  int port = host_port_pair.port();
  ASSERT_GT(port, 0);

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser_context());

  ExtensionTestMessageListener listener("info_please",
                                        ReplyBehavior::kWillReply);

  ASSERT_TRUE(LoadApp("sockets_udp/api"));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(base::StringPrintf("udp_send_write_quota:%s:%d",
                                    host_port_pair.host().c_str(), port));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
