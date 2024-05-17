// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/direct_sockets/direct_sockets_service_impl.h"
#include "content/browser/direct_sockets/direct_sockets_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_udp_socket.h"
#include "services/network/test/udp_socket_test_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/permission_broker/fake_permission_broker_client.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS)

// The tests in this file use the Network Service implementation of
// NetworkContext, to test sending and receiving of data over UDP sockets.

namespace content {

namespace {

constexpr char kLocalhostAddress[] = "127.0.0.1";

}  // anonymous namespace

class DirectSocketsUdpBrowserTest : public ContentBrowserTest {
 public:
  ~DirectSocketsUdpBrowserTest() override = default;

  GURL GetTestPageURL() {
    return embedded_test_server()->GetURL("/direct_sockets/udp.html");
  }

  network::mojom::NetworkContext* GetNetworkContext() {
    return browser_context()->GetDefaultStoragePartition()->GetNetworkContext();
  }

  raw_ptr<content::test::AsyncJsRunner> GetAsyncJsRunner() const {
    return runner_.get();
  }

  void ConnectJsSocket(int port = 0) const {
    const std::string open_socket = JsReplace(
        R"(
          socket = new UDPSocket({ remoteAddress: $1, remotePort: $2 });
          await socket.opened;
        )",
        kLocalhostAddress, port);

    ASSERT_TRUE(
        EvalJs(shell(), content::test::WrapAsync(open_socket)).value.is_none());
  }

 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    client_ = std::make_unique<test::IsolatedWebAppContentBrowserClient>(
        url::Origin::Create(GetTestPageURL()));
    runner_ =
        std::make_unique<content::test::AsyncJsRunner>(shell()->web_contents());

    ASSERT_TRUE(NavigateToURL(shell(), GetTestPageURL()));
  }

  void SetUp() override {
    embedded_test_server()->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(embedded_test_server()->Start());

    ContentBrowserTest::SetUp();
  }

  std::pair<net::IPEndPoint,
            std::unique_ptr<network::test::UDPSocketTestHelper>>
  CreateUDPServerSocket(mojo::PendingRemote<network::mojom::UDPSocketListener>
                            listener_receiver_remote) {
    GetNetworkContext()->CreateUDPSocket(
        server_socket_.BindNewPipeAndPassReceiver(),
        std::move(listener_receiver_remote));

    server_socket_.set_disconnect_handler(
        base::BindLambdaForTesting([]() { NOTREACHED_IN_MIGRATION(); }));

    net::IPEndPoint server_addr(net::IPAddress::IPv4Localhost(), 0);
    auto server_helper =
        std::make_unique<network::test::UDPSocketTestHelper>(&server_socket_);
    int result = server_helper->BindSync(server_addr, nullptr, &server_addr);
    DCHECK_EQ(net::OK, result);
    return {server_addr, std::move(server_helper)};
  }

  mojo::Remote<network::mojom::UDPSocket>& GetUDPServerSocket() {
    return server_socket_;
  }

 private:
  BrowserContext* browser_context() {
    return shell()->web_contents()->GetBrowserContext();
  }

  base::test::ScopedFeatureList feature_list_{blink::features::kDirectSockets};
  mojo::Remote<network::mojom::UDPSocket> server_socket_;

  std::unique_ptr<test::IsolatedWebAppContentBrowserClient> client_;
  std::unique_ptr<content::test::AsyncJsRunner> runner_;
};

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpBrowserTest, CloseUdp) {
  const std::string script =
      "closeUdp({ remoteAddress: '::1', remotePort: 993 })";

  EXPECT_EQ("closeUdp succeeded", EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpBrowserTest, SendUdpAfterClose) {
  const int32_t kRequiredBytes = 1;
  const std::string script =
      JsReplace("sendUdpAfterClose({ remoteAddress: $1, remotePort: $2 }, $3)",
                kLocalhostAddress, 993, kRequiredBytes);

  EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
              ::testing::HasSubstr("Stream closed."));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpBrowserTest, ReadUdpAfterSocketClose) {
  network::test::UDPSocketListenerImpl listener;
  mojo::Receiver<network::mojom::UDPSocketListener> listener_receiver{
      &listener};

  auto [server_address, server_helper] =
      CreateUDPServerSocket(listener_receiver.BindNewPipeAndPassRemote());

  const std::string script = JsReplace(
      "readUdpAfterSocketClose({ remoteAddress: $1, remotePort: $2 })",
      server_address.ToStringWithoutPort(), server_address.port());

  EXPECT_EQ("readUdpAferSocketClose succeeded.", EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpBrowserTest, ReadUdpAfterStreamClose) {
  network::test::UDPSocketListenerImpl listener;
  mojo::Receiver<network::mojom::UDPSocketListener> listener_receiver{
      &listener};

  auto [server_address, server_helper] =
      CreateUDPServerSocket(listener_receiver.BindNewPipeAndPassRemote());

  const std::string script = JsReplace(
      "readUdpAfterStreamClose({ remoteAddress: $1, remotePort: $2 })",
      server_address.ToStringWithoutPort(), server_address.port());

  EXPECT_EQ("readUdpAferStreamClose succeeded.", EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpBrowserTest, CloseWithActiveReader) {
  network::test::UDPSocketListenerImpl listener;
  mojo::Receiver<network::mojom::UDPSocketListener> listener_receiver{
      &listener};

  auto [server_address, server_helper] =
      CreateUDPServerSocket(listener_receiver.BindNewPipeAndPassRemote());

  const std::string open_socket = JsReplace(
      "closeUdpWithLockedReadable({ remoteAddress: $1, remotePort: $2 }, "
      "/*unlock=*/false)",
      server_address.ToStringWithoutPort(), server_address.port());

  EXPECT_THAT(EvalJs(shell(), open_socket).ExtractString(),
              ::testing::StartsWith("closeUdpWithLockedReadable failed"));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpBrowserTest,
                       CloseWithActiveReaderForce) {
  network::test::UDPSocketListenerImpl listener;
  mojo::Receiver<network::mojom::UDPSocketListener> listener_receiver{
      &listener};

  auto [server_address, server_helper] =
      CreateUDPServerSocket(listener_receiver.BindNewPipeAndPassRemote());

  const std::string open_socket = JsReplace(
      "closeUdpWithLockedReadable({ remoteAddress: $1, remotePort: $2 }, "
      "/*unlock=*/true)",
      server_address.ToStringWithoutPort(), server_address.port());

  EXPECT_THAT(EvalJs(shell(), open_socket).ExtractString(),
              ::testing::StartsWith("closeUdpWithLockedReadable succeeded"));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpBrowserTest, ReadWriteUdpOnSendError) {
  content::test::MockNetworkContext mock_network_context;
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  ConnectJsSocket();

  const std::string async_read = "readWriteUdpOnError(socket);";
  auto future = GetAsyncJsRunner()->RunScript(async_read);

  // Next attempt to write to the socket will result in ERR_UNEXPECTED and close
  // the writable stream.
  mock_network_context.get_udp_socket()->SetNextSendResult(net::ERR_UNEXPECTED);

  // MockNetworkContext owns the MockUDPSocket and therefore outlives it.
  mock_network_context.get_udp_socket()->SetAdditionalSendCallback(
      base::BindOnce(
          [](content::test::MockNetworkContext* context) {
            // Next read from the socket will receive ERR_UNEXPECTED and close
            // the readable stream.
            context->get_udp_socket()->MockSend(net::ERR_UNEXPECTED);
          },
          &mock_network_context));

  EXPECT_THAT(future->Get(),
              ::testing::HasSubstr("readWriteUdpOnError succeeded"));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpBrowserTest, ReadWriteUdpOnSocketError) {
  content::test::MockNetworkContext mock_network_context;
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  ConnectJsSocket();

  // Next attempt to write to the socket will result in ERR_UNEXPECTED and close
  // the writable stream.
  mock_network_context.get_udp_socket()->SetNextSendResult(net::ERR_UNEXPECTED);

  // MockNetworkContext owns the MockUDPSocket and therefore outlives it.
  mock_network_context.get_udp_socket()->SetAdditionalSendCallback(
      base::BindOnce(
          [](content::test::MockNetworkContext* context) {
            // This will break the receiver pipe and close the readable stream.
            context->get_udp_socket()->get_listener().reset();
          },
          &mock_network_context));

  const std::string script = "readWriteUdpOnError(socket)";
  auto future = GetAsyncJsRunner()->RunScript(script);

  EXPECT_THAT(future->Get(),
              ::testing::HasSubstr("readWriteUdpOnError succeeded"));
}

class DirectSocketsBoundUdpBrowserTest : public DirectSocketsUdpBrowserTest {
 public:
#if BUILDFLAG(IS_CHROMEOS)
  DirectSocketsBoundUdpBrowserTest() {
    chromeos::PermissionBrokerClient::InitializeFake();
    DirectSocketsServiceImpl::SetAlwaysOpenFirewallHoleForTesting();
  }

  ~DirectSocketsBoundUdpBrowserTest() override {
    chromeos::PermissionBrokerClient::Shutdown();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
};

IN_PROC_BROWSER_TEST_F(DirectSocketsBoundUdpBrowserTest, ExchangeUdp) {
  ASSERT_THAT(EvalJs(shell(), "exchangeUdpPacketsBetweenClientAndServer()")
                  .ExtractString(),
              testing::HasSubstr("succeeded"));
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(DirectSocketsBoundUdpBrowserTest, HasFirewallHole) {
  class DelegateImpl : public chromeos::FakePermissionBrokerClient::Delegate {
   public:
    DelegateImpl(uint16_t port, base::OnceClosure quit_closure)
        : port_(port), quit_closure_(std::move(quit_closure)) {}

    void OnUdpPortReleased(uint16_t port,
                           const std::string& interface) override {
      if (port == port_) {
        ASSERT_EQ(interface, "");
        ASSERT_TRUE(quit_closure_);
        std::move(quit_closure_).Run();
      }
    }

   private:
    uint16_t port_;
    base::OnceClosure quit_closure_;
  };

  auto* client = static_cast<chromeos::FakePermissionBrokerClient*>(
      chromeos::PermissionBrokerClient::Get());

  const std::string open_script = R"(
    (async () => {
      socket = new UDPSocket({ localAddress: '127.0.0.1' });
      const { localPort } = await socket.opened;
      return localPort;
    })();
  )";

  const int32_t local_port = EvalJs(shell(), open_script).ExtractInt();
  ASSERT_TRUE(client->HasUdpHole(local_port, "" /* all interfaces */));

  base::RunLoop run_loop;
  auto delegate =
      std::make_unique<DelegateImpl>(local_port, run_loop.QuitClosure());
  client->AttachDelegate(delegate.get());

  EXPECT_TRUE(EvalJs(shell(), content::test::WrapAsync("socket.close()"))
                  .error.empty());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBoundUdpBrowserTest, FirewallHoleDenied) {
  auto* client = chromeos::FakePermissionBrokerClient::Get();
  client->SetUdpDenyAll();

  const std::string open_script = R"(
    (async () => {
      socket = new UDPSocket({ localAddress: '127.0.0.1' });
      return await socket.opened.catch(err => err.message);
    })();
  )";

  EXPECT_THAT(EvalJs(shell(), open_script).ExtractString(),
              testing::HasSubstr("Firewall"));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpBrowserTest, UdpMessageConfigurations) {
  {
    const std::string script = R"(
      testUdpMessageConfiguration({
        localAddress: '127.0.0.1',
      }, {})
    )";
    ASSERT_THAT(EvalJs(shell(), script).ExtractString(),
                testing::HasSubstr("UDPMessage: missing 'data' field"));
  }

  {
    const std::string script = R"(
      testUdpMessageConfiguration({
        localAddress: '127.0.0.1',
      }, {
        data: (new TextEncoder()).encode("meow"),
        remoteAddress: '127.0.0.1',
      })
    )";
    ASSERT_THAT(EvalJs(shell(), script).ExtractString(),
                testing::HasSubstr("UDPMessage: either none or both "
                                   "'remoteAddress' and 'remotePort'"));
  }

  {
    const std::string script = R"(
      testUdpMessageConfiguration({
        localAddress: '127.0.0.1',
      }, {
        data: (new TextEncoder()).encode("meow"),
        remotePort: 53,
      })
    )";
    ASSERT_THAT(EvalJs(shell(), script).ExtractString(),
                testing::HasSubstr("UDPMessage: either none or both "
                                   "'remoteAddress' and 'remotePort'"));
  }

  {
    const std::string script = R"(
      testUdpMessageConfiguration({
        localAddress: '127.0.0.1',
      }, {
        data: (new TextEncoder()).encode("meow"),
      })
    )";
    ASSERT_THAT(
        EvalJs(shell(), script).ExtractString(),
        testing::HasSubstr(
            "UDPMessage: 'remoteAddress' and 'remotePort' must be specified"));
  }

  {
    const std::string script = R"(
      testUdpMessageConfiguration({
        localAddress: '127.0.0.1',
      }, {
        data: (new TextEncoder()).encode("meow"),
      })
    )";
    ASSERT_THAT(
        EvalJs(shell(), script).ExtractString(),
        testing::HasSubstr("UDPMessage: 'remoteAddress' and 'remotePort' must "
                           "be specified in 'bound'"));
  }

  {
    const std::string script = R"(
      testUdpMessageConfiguration({
        remoteAddress: '127.0.0.1',
        remotePort: 53,
      }, {
        data: (new TextEncoder()).encode("meow"),
        remoteAddress: '127.0.0.1',
        remotePort: 53,
      })
    )";
    ASSERT_THAT(EvalJs(shell(), script).ExtractString(),
                testing::HasSubstr(
                    "UDPMessage: 'remoteAddress' and "
                    "'remotePort' must not be specified in 'connected'"));
  }
}

}  // namespace content
