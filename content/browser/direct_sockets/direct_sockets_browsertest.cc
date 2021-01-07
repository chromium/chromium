// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <vector>

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
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
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "url/gurl.h"
#include "url/url_canon_ip.h"

using testing::StartsWith;

namespace content {

namespace {

struct RecordedCall {
  DirectSocketsServiceImpl::ProtocolType protocol_type;

  std::string remote_address;
  uint16_t remote_port;

  int32_t send_buffer_size = 0;
  int32_t receive_buffer_size = 0;

  bool no_delay = false;
};

class MockHostResolver : public network::mojom::HostResolver {
 public:
  explicit MockHostResolver(
      mojo::PendingReceiver<network::mojom::HostResolver> resolver_receiver)
      : receiver_(this) {
    receiver_.Bind(std::move(resolver_receiver));
  }

  MockHostResolver(const MockHostResolver&) = delete;
  MockHostResolver& operator=(const MockHostResolver&) = delete;

  static std::map<std::string, std::string>& known_hosts() {
    static base::NoDestructor<std::map<std::string, std::string>> hosts;
    return *hosts;
  }

  void ResolveHost(const ::net::HostPortPair& host_port_pair,
                   const ::net::NetworkIsolationKey& network_isolation_key,
                   network::mojom::ResolveHostParametersPtr optional_parameters,
                   ::mojo::PendingRemote<network::mojom::ResolveHostClient>
                       pending_response_client) override {
    mojo::Remote<network::mojom::ResolveHostClient> response_client(
        std::move(pending_response_client));

    std::string host = host_port_pair.host();
    auto iter = known_hosts().find(host);
    if (iter != known_hosts().end())
      host = iter->second;

    net::IPAddress remote_address;
    // TODO(crbug.com/1141241): Replace if/else with AssignFromIPLiteral.
    if (host.find(':') != std::string::npos) {
      // GURL expects IPv6 hostnames to be surrounded with brackets.
      std::string host_brackets = base::StrCat({"[", host, "]"});
      url::Component host_comp(0, host_brackets.size());
      std::array<uint8_t, 16> bytes;
      EXPECT_TRUE(url::IPv6AddressToNumber(host_brackets.data(), host_comp,
                                           bytes.data()));
      remote_address = net::IPAddress(bytes.data(), bytes.size());
    } else {
      // Otherwise the string is an IPv4 address.
      url::Component host_comp(0, host.size());
      std::array<uint8_t, 4> bytes;
      int num_components;
      url::CanonHostInfo::Family family = url::IPv4AddressToNumber(
          host.data(), host_comp, bytes.data(), &num_components);
      EXPECT_EQ(family, url::CanonHostInfo::IPV4);
      EXPECT_EQ(num_components, 4);
      remote_address = net::IPAddress(bytes.data(), bytes.size());
    }
    EXPECT_EQ(remote_address.ToString(), host);

    response_client->OnComplete(net::OK, net::ResolveErrorInfo(),
                                net::AddressList::CreateFromIPAddress(
                                    remote_address, host_port_pair.port()));
  }

  void MdnsListen(
      const ::net::HostPortPair& host,
      ::net::DnsQueryType query_type,
      ::mojo::PendingRemote<network::mojom::MdnsListenClient> response_client,
      MdnsListenCallback callback) override {
    NOTIMPLEMENTED();
  }

 private:
  mojo::Receiver<network::mojom::HostResolver> receiver_;
};

class MockNetworkContext : public network::TestNetworkContext {
 public:
  explicit MockNetworkContext(net::Error result) : result_(result) {}

  MockNetworkContext(const MockNetworkContext&) = delete;
  MockNetworkContext& operator=(const MockNetworkContext&) = delete;

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
    const net::IPEndPoint& peer_addr = remote_addr_list.front();
    history_.push_back(
        RecordedCall{DirectSocketsServiceImpl::ProtocolType::kTcp,
                     peer_addr.address().ToString(), peer_addr.port(),
                     tcp_connected_socket_options->send_buffer_size,
                     tcp_connected_socket_options->receive_buffer_size,
                     tcp_connected_socket_options->no_delay});

    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    DCHECK_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr, &producer, &consumer));
    std::move(callback).Run(result_, local_addr, peer_addr, std::move(consumer),
                            std::move(producer));
  }

  void CreateHostResolver(
      const base::Optional<net::DnsConfigOverrides>& config_overrides,
      mojo::PendingReceiver<network::mojom::HostResolver> receiver) override {
    DCHECK(!config_overrides.has_value());
    DCHECK(!host_resolver_);
    host_resolver_ = std::make_unique<MockHostResolver>(std::move(receiver));
  }

 private:
  const net::Error result_;
  std::vector<RecordedCall> history_;
  std::unique_ptr<network::mojom::HostResolver> host_resolver_;
};

net::Error UnconditionallyPermitConnection(
    const blink::mojom::DirectSocketOptions& options) {
  DCHECK(options.remote_hostname.has_value());
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

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenTcp_Success) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  const uint16_t listening_port = StartTcpServer();
  const std::string script = base::StringPrintf(
      "openTcp({remoteAddress: '127.0.0.1', remotePort: %d})", listening_port);

  EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
              StartsWith("openTcp succeeded"));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenTcp_Success_Global) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  const uint16_t listening_port = StartTcpServer();
  const std::string script = base::StringPrintf(
      "openTcp({remoteAddress: '127.0.0.1', remotePort: %d})", listening_port);

  EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
              StartsWith("openTcp succeeded"));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenTcp_Success_Hostname) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  const char kExampleHostname[] = "mail.example.com";
  const char kExampleAddress[] = "98.76.54.32";
  MockHostResolver::known_hosts()[kExampleHostname] = kExampleAddress;

  MockNetworkContext mock_network_context(net::OK);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);
  const std::string expected_result = base::StringPrintf(
      "openTcp succeeded: {remoteAddress: \"%s\", remotePort: 993}",
      kExampleAddress);

  const std::string script = base::StringPrintf(
      "openTcp({remoteAddress: '%s', remotePort: 993})", kExampleHostname);

  EXPECT_EQ(expected_result, EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenTcp_CannotEvadeCors) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  // HTTPS uses port 443.
  const std::string script =
      "openTcp({remoteAddress: '127.0.0.1', remotePort: 443})";

  EXPECT_EQ("openTcp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenTcp_OptionsOne) {
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
  EXPECT_EQ(DirectSocketsServiceImpl::ProtocolType::kTcp, call.protocol_type);
  EXPECT_EQ("12.34.56.78", call.remote_address);
  EXPECT_EQ(9012, call.remote_port);
  EXPECT_EQ(3456, call.send_buffer_size);
  EXPECT_EQ(7890, call.receive_buffer_size);
  EXPECT_EQ(false, call.no_delay);
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenTcp_OptionsTwo) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  MockNetworkContext mock_network_context(net::OK);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

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
  EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
              StartsWith("openTcp succeeded"));

  DCHECK_EQ(1U, mock_network_context.history().size());
  const RecordedCall& call = mock_network_context.history()[0];
  EXPECT_EQ(DirectSocketsServiceImpl::ProtocolType::kTcp, call.protocol_type);
  EXPECT_EQ("fedc:ba98:7654:3210:fedc:ba98:7654:3210", call.remote_address);
  EXPECT_EQ(789, call.remote_port);
  EXPECT_EQ(0, call.send_buffer_size);
  EXPECT_EQ(1234, call.receive_buffer_size);
  EXPECT_EQ(true, call.no_delay);
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, CloseTcp) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  const uint16_t listening_port = StartTcpServer();
  const std::string script = base::StringPrintf(
      "closeTcp({remoteAddress: '127.0.0.1', remotePort: %d})", listening_port);

  EXPECT_EQ("closeTcp succeeded", EvalJs(shell(), script));
}

// Tests that we can close the writer, then the socket.
IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, CloseTcpWriter) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  const uint16_t listening_port = StartTcpServer();
  const std::string script = base::StringPrintf(
      "closeTcp({remoteAddress: '127.0.0.1', remotePort: %d}, "
      "/*closeWriter=*/true)",
      listening_port);

  EXPECT_EQ("closeTcp succeeded", EvalJs(shell(), script));
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
