// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "extensions/browser/api/sockets_udp/sockets_udp_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/shell/test/shell_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

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

  std::unique_ptr<base::Value> result(
      api_test_utils::RunFunctionAndReturnSingleResult(
          socket_create_function.get(), "[]", browser_context()));

  base::DictionaryValue* value = NULL;
  ASSERT_TRUE(result->GetAsDictionary(&value));
  int socketId = -1;
  EXPECT_TRUE(value->GetInteger("socketId", &socketId));
  ASSERT_TRUE(socketId > 0);
}

// Disable SocketsUdpExtension on Mac due to time out.
// See https://crbug.com/844402.
// Disable on Linux for flakiness. See https://crbug.com/875920.
#if defined(OS_MACOSX) || defined(OS_LINUX)
#define MAYBE_SocketsUdpExtension DISABLED_SocketsUdpExtension
#else
#define MAYBE_SocketsUdpExtension SocketsUdpExtension
#endif

IN_PROC_BROWSER_TEST_F(SocketsUdpApiTest, MAYBE_SocketsUdpExtension) {
  std::unique_ptr<net::SpawnedTestServer> test_server(
      new net::SpawnedTestServer(
          net::SpawnedTestServer::TYPE_UDP_ECHO,
          base::FilePath(FILE_PATH_LITERAL("net/data"))));
  EXPECT_TRUE(test_server->Start());

  net::HostPortPair host_port_pair = test_server->host_port_pair();
  int port = host_port_pair.port();
  ASSERT_TRUE(port > 0);

  // Test that sendTo() is properly resolving hostnames.
  host_port_pair.set_host(kHostname);

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser_context());

  ExtensionTestMessageListener listener("info_please", true);

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
  ExtensionTestMessageListener listener("info_please", true);
  ASSERT_TRUE(LoadApp("sockets_udp/api"));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(base::StringPrintf("multicast:%s:%d", kHostname, kPort));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
