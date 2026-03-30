// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "chromeos/dbus/permission_broker/fake_permission_broker_client.h"
#include "content/browser/direct_sockets/direct_sockets_test_utils.h"
#include "content/browser/direct_sockets/firewall_hole_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

class AsyncPermissionBrokerClient
    : public chromeos::FakePermissionBrokerClient {
 public:
  AsyncPermissionBrokerClient() = default;
  ~AsyncPermissionBrokerClient() override = default;

  void RequestUdpPortAccess(uint16_t port,
                            const std::string& interface,
                            int lifeline_fd,
                            ResultCallback callback) override {
    // Delay the response significantly to allow the renderer to flood the
    // browser with concurrent requests for the same port.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &AsyncPermissionBrokerClient::CompleteRequestUdpPortAccess,
            weak_factory_.GetWeakPtr(), port, interface, std::move(callback)),
        base::Milliseconds(100));
  }

 private:
  void CompleteRequestUdpPortAccess(uint16_t port,
                                    const std::string& interface,
                                    ResultCallback callback) {
    chromeos::FakePermissionBrokerClient::RequestUdpPortAccess(
        port, interface, -1 /* lifeline_fd */, std::move(callback));
  }

  base::WeakPtrFactory<AsyncPermissionBrokerClient> weak_factory_{this};
};

}  // namespace

class FirewallHoleBrowserTest : public ContentBrowserTest {
 public:
  FirewallHoleBrowserTest() {
    // Manually initialize our async fake client.
    new AsyncPermissionBrokerClient();
    FirewallHoleDelegate::SetAlwaysOpenFirewallHoleForTesting(true);
  }

  ~FirewallHoleBrowserTest() override {
    FirewallHoleDelegate::SetAlwaysOpenFirewallHoleForTesting(false);
    if (chromeos::PermissionBrokerClient::Get()) {
      chromeos::PermissionBrokerClient::Shutdown();
    }
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    client_ = std::make_unique<test::IsolatedWebAppContentBrowserClient>(
        url::Origin::Create(GetTestPageURL()));
    ASSERT_TRUE(NavigateToURL(shell(), GetTestPageURL()));
  }

  void SetUp() override {
    embedded_test_server()->AddDefaultHandlers();
    ASSERT_TRUE(embedded_test_server()->Start());
    ContentBrowserTest::SetUp();
  }

  GURL GetTestPageURL() {
    return test::FileWithHeaders("/direct_sockets/udp.html")
        .WithCOIHeaders()
        .WithPermissionsPolicy("cross-origin-isolated", "(self)")
        .WithPermissionsPolicy("direct-sockets", "(self)")
        .WithPermissionsPolicy("direct-sockets-private", "(self)")
        .WithPermissionsPolicy("direct-sockets-multicast", "(self)")
        .Build(embedded_test_server());
  }

  AsyncPermissionBrokerClient* GetAsyncFakeClient() {
    return static_cast<AsyncPermissionBrokerClient*>(
        chromeos::PermissionBrokerClient::Get());
  }

 protected:
  std::unique_ptr<test::IsolatedWebAppContentBrowserClient> client_;
};

// Test for https://g-issues.chromium.org/issues/496382601
// verifying that opening many concurrent `UDPSocket` on the
// same port on chrome os with `multicastAllowAddressSharing: true`
// will not leave hanging firewall hole.
IN_PROC_BROWSER_TEST_F(FirewallHoleBrowserTest,
                       LeakUdpFirewallHoleOnConcurrentRequests) {
  const uint16_t kTestPort = 12345;
  const int kNumSockets = 100;

  const std::string open_sockets_script =
      base::StringPrintf(R"(
    (async () => {
      const options = {
        localAddress: '127.0.0.1',
        localPort: %u,
        multicastAllowAddressSharing: true
      };

      const sockets = [];
      const opened_promises = [];
      for (let i = 0; i < %d; ++i) {
        const s = new UDPSocket(options);
        sockets.push(s);
        opened_promises.push(s.opened);
      }

      await Promise.all(opened_promises);
      window.sockets = sockets;
      return true;
    })();
  )",
                         kTestPort, kNumSockets);

  ASSERT_TRUE(EvalJs(shell(), open_sockets_script).ExtractBool());

  ASSERT_TRUE(GetAsyncFakeClient()->HasUdpHole(kTestPort, ""));

  const std::string close_sockets_script = R"(
    (async () => {
      for (const s of window.sockets) {
        await s.close();
      }
      return true;
    })();
  )";

  ASSERT_TRUE(EvalJs(shell(), close_sockets_script).ExtractBool());

  // Wait for Mojo disconnect handlers and tasks to run in the browser process.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(200));
  run_loop.Run();
  base::RunLoop().RunUntilIdle();

  // Check that firewall hole is closed.
  EXPECT_FALSE(GetAsyncFakeClient()->HasUdpHole(kTestPort, ""));
}

}  // namespace content
