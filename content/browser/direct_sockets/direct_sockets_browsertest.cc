// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/direct_sockets/direct_sockets_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/test/test_network_context.h"
#include "url/gurl.h"

namespace content {

namespace {

enum class ProtocolType { kTcp, kUdp };

struct RecordedCall {
  ProtocolType protocol_type;

  std::string remote_address;
  uint16_t remote_port;

  int32_t send_buffer_size = 0;
  int32_t receive_buffer_size = 0;

  bool no_delay = false;
};

class MockNetworkContext : public network::TestNetworkContext {
 public:
  explicit MockNetworkContext(net::Error result) : result_(result) {}

  const std::vector<RecordedCall>& history() const { return history_; }

  // network::TestNetworkContext:
  void CreateTCPConnectedSocket(
      const base::Optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      network::mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      CreateTCPConnectedSocketCallback callback) override {
    history_.push_back(RecordedCall{
        ProtocolType::kTcp, remote_addr_list[0].address().ToString(),
        remote_addr_list[0].port(),
        tcp_connected_socket_options->send_buffer_size,
        tcp_connected_socket_options->receive_buffer_size,
        tcp_connected_socket_options->no_delay});

    std::move(callback).Run(result_, base::nullopt, base::nullopt,
                            mojo::ScopedDataPipeConsumerHandle(),
                            mojo::ScopedDataPipeProducerHandle());
  }

 private:
  const net::Error result_;
  std::vector<RecordedCall> history_;
};

net::Error UnconditionallyPermitConnection(
    const blink::mojom::DirectSocketOptions& options,
    net::IPAddress& remote_address) {
  DCHECK(options.remote_hostname.has_value());
  DCHECK(remote_address.AssignFromIPLiteral(*options.remote_hostname));
  EXPECT_EQ(remote_address.ToString(), *options.remote_hostname);
  return net::OK;
}

}  // anonymous namespace

class DirectSocketsBrowserTest : public ContentBrowserTest {
 public:
  DirectSocketsBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kDirectSockets);
  }
  ~DirectSocketsBrowserTest() override = default;

  GURL GetTestPageURL() {
    return embedded_test_server()->GetURL("/direct_sockets/index.html");
  }

  network::mojom::NetworkContext* GetNetworkContext() {
    return BrowserContext::GetDefaultStoragePartition(browser_context())
        ->GetNetworkContext();
  }

  // Returns the port listening for TCP connections.
  uint16_t StartTcpServer() {
    net::IPEndPoint local_addr;
    base::RunLoop run_loop;
    GetNetworkContext()->CreateTCPServerSocket(
        net::IPEndPoint(net::IPAddress::IPv4Localhost(),
                        /*port=*/0),
        /*backlog=*/5,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        tcp_server_socket_.BindNewPipeAndPassReceiver(),
        base::BindLambdaForTesting(
            [&local_addr, &run_loop](
                int32_t result,
                const base::Optional<net::IPEndPoint>& local_addr_out) {
              DCHECK_EQ(result, net::OK);
              DCHECK(local_addr_out.has_value());
              local_addr = *local_addr_out;
              run_loop.Quit();
            }));
    run_loop.Run();
    return local_addr.port();
  }

 protected:
  void SetUp() override {
    embedded_test_server()->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(embedded_test_server()->Start());

    ContentBrowserTest::SetUp();
  }

 private:
  BrowserContext* browser_context() {
    return shell()->web_contents()->GetBrowserContext();
  }

  base::test::ScopedFeatureList feature_list_;
  mojo::Remote<network::mojom::TCPServerSocket> tcp_server_socket_;
};

// TODO(crbug.com/1141241): Resolve failures on linux-bfcache-rel bots.
IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, DISABLED_OpenTcp_Success) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  const uint16_t listening_port = StartTcpServer();
  const std::string script = base::StringPrintf(
      "openTcp({remoteAddress: '127.0.0.1', remotePort: %d})", listening_port);

  EXPECT_EQ("openTcp succeeded", EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenTcp_NotAllowedError) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  const uint16_t listening_port = StartTcpServer();
  const std::string script = base::StringPrintf(
      "openTcp({remoteAddress: '127.0.0.1', remotePort: %d})", listening_port);

  EXPECT_EQ("openTcp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenTcp_CannotEvadeCors) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  // HTTPS uses port 443.
  const std::string script =
      "openTcp({remoteAddress: '127.0.0.1', remotePort: 443})";

  EXPECT_EQ("openTcp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
}

// TODO(crbug.com/1141241): Resolve failures on linux-bfcache-rel bots.
IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, DISABLED_OpenTcp_OptionsOne) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  MockNetworkContext mock_network_context(net::ERR_PROXY_CONNECTION_FAILED);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);
  const std::string expected_result =
      "openTcp failed: NotAllowedError: Permission denied";

  const std::string script =
      R"(
          openTcp({
            remoteAddress: '12.34.56.78',
            remotePort: 9012,
            sendBufferSize: 3456,
            receiveBufferSize: 7890,
            noDelay: false
          })
        )";
  EXPECT_EQ(expected_result, EvalJs(shell(), script));

  DCHECK_EQ(1U, mock_network_context.history().size());
  const RecordedCall& call = mock_network_context.history()[0];
  EXPECT_EQ(ProtocolType::kTcp, call.protocol_type);
  EXPECT_EQ("12.34.56.78", call.remote_address);
  EXPECT_EQ(9012, call.remote_port);
  EXPECT_EQ(3456, call.send_buffer_size);
  EXPECT_EQ(7890, call.receive_buffer_size);
  EXPECT_EQ(false, call.no_delay);
}

// TODO(crbug.com/1141241): Resolve failures on linux-bfcache-rel bots.
IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, DISABLED_OpenTcp_OptionsTwo) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  MockNetworkContext mock_network_context(net::OK);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);
  const std::string expected_result = "openTcp succeeded";

  const std::string script =
      R"(
          openTcp({
            remoteAddress: 'fedc:ba98:7654:3210:fedc:ba98:7654:3210',
            remotePort: 789,
            sendBufferSize: 0,
            receiveBufferSize: 1234,
            noDelay: true
          })
        )";
  EXPECT_EQ(expected_result, EvalJs(shell(), script));

  DCHECK_EQ(1U, mock_network_context.history().size());
  const RecordedCall& call = mock_network_context.history()[0];
  EXPECT_EQ(ProtocolType::kTcp, call.protocol_type);
  EXPECT_EQ("fedc:ba98:7654:3210:fedc:ba98:7654:3210", call.remote_address);
  EXPECT_EQ(789, call.remote_port);
  EXPECT_EQ(0, call.send_buffer_size);
  EXPECT_EQ(1234, call.receive_buffer_size);
  EXPECT_EQ(true, call.no_delay);
}

// TODO(crbug.com/1141241): Resolve failures on linux-bfcache-rel bots.
IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, DISABLED_OpenUdp_Success) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  // TODO(crbug.com/1119620): Use port from a listening net::UDPServerSocket.
  const std::string script = base::StringPrintf(
      "openUdp({remoteAddress: '127.0.0.1', remotePort: %d})", 0);

  EXPECT_EQ("openUdp succeeded", EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenUdp_NotAllowedError) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  // TODO(crbug.com/1119620): Use port from a listening net::UDPServerSocket.
  const std::string script = base::StringPrintf(
      "openUdp({remoteAddress: '127.0.0.1', remotePort: %d})", 0);

  EXPECT_EQ("openUdp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenUdp_CannotEvadeCors) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  // QUIC uses port 443.
  const std::string script =
      "openUdp({remoteAddress: '127.0.0.1', remotePort: 443})";

  EXPECT_EQ("openUdp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
}

}  // namespace content
