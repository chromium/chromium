// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/direct_sockets/direct_sockets_service_impl.h"
#include "content/browser/direct_sockets/direct_sockets_test_utils.h"
#include "content/browser/direct_sockets/resolve_host_and_open_socket.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
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
#include "net/dns/host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/net_buildflags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_udp_socket.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

// The tests in this file use a mock implementation of NetworkContext, to test
// DNS resolving, and the opening of TCP and UDP sockets.

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

  network::mojom::TCPKeepAliveOptionsPtr keep_alive_options;
};

constexpr char kLocalhostAddress[] = "127.0.0.1";

constexpr char kPermissionDeniedHistogramName[] =
    "DirectSockets.PermissionDeniedFailures";

constexpr char kTCPNetworkFailuresHistogramName[] =
    "DirectSockets.TCPNetworkFailures";

constexpr char kUDPNetworkFailuresHistogramName[] =
    "DirectSockets.UDPNetworkFailures";

const std::string kIPv4_tests[] = {
    // 0.0.0.0/8
    "0.0.0.0", "0.255.255.255",
    // 10.0.0.0/8
    "10.0.0.0", "10.255.255.255",
    // 100.64.0.0/10
    "100.64.0.0", "100.127.255.255",
    // 127.0.0.0/8
    "127.0.0.0", "127.255.255.255",
    // 169.254.0.0/16
    "169.254.0.0", "169.254.255.255",
    // 172.16.0.0/12
    "172.16.0.0", "172.31.255.255",
    // 192.0.2.0/24
    "192.0.2.0", "192.0.2.255",
    // 192.88.99.0/24
    "192.88.99.0", "192.88.99.255",
    // 192.168.0.0/16
    "192.168.0.0", "192.168.255.255",
    // 198.18.0.0/15
    "198.18.0.0", "198.19.255.255",
    // 198.51.100.0/24
    "198.51.100.0", "198.51.100.255",
    // 203.0.113.0/24
    "203.0.113.0", "203.0.113.255",
    // 224.0.0.0/8 - 255.0.0.0/8
    "224.0.0.0", "255.255.255.255"};

const std::string kIPv6_tests[] = {
    // 0000::/8.
    // Skip testing ::ffff:/96 explicitly since it will be tested through
    // mapping Ipv4 Addresses.
    "0:0:0:0:0:0:0:0", "ff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
    // 0100::/8
    "100:0:0:0:0:0:0:0", "1ff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
    // 0200::/7
    "200:0:0:0:0:0:0:0", "3ff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
    // 0400::/6
    "400:0:0:0:0:0:0:0", "7ff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
    // 0800::/5
    "800:0:0:0:0:0:0:0", "fff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
    // 1000::/4
    "1000:0:0:0:0:0:0:0", "1fff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
    // 4000::/3
    "4000:0:0:0:0:0:0:0", "5fff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
    // 6000::/3
    "6000:0:0:0:0:0:0:0", "7fff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
    // 8000::/3
    "8000:0:0:0:0:0:0:0", "9fff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
    // c000::/3
    "c000:0:0:0:0:0:0:0", "dfff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
    // e000::/4
    "e000:0:0:0:0:0:0:0", "efff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
    // f000::/5
    "f000:0:0:0:0:0:0:0", "f7ff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
    // f800::/6
    "f800:0:0:0:0:0:0:0", "fbff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
    // fc00::/7
    "fc00:0:0:0:0:0:0:0", "fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
    // fe00::/9
    "fe00:0:0:0:0:0:0:0", "fe7f:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
    // fe80::/10
    "fe80:0:0:0:0:0:0:0", "febf:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
    // fec0::/10
    "fec0:0:0:0:0:0:0:0", "feff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"};

class MockOpenNetworkContext : public content::test::MockNetworkContext {
 public:
  explicit MockOpenNetworkContext(net::Error result) : result_(result) {}

  ~MockOpenNetworkContext() override = default;

  void Record(RecordedCall call) { history_.push_back(std::move(call)); }

  net::Error result() const { return result_; }

  const std::vector<RecordedCall>& history() const { return history_; }

  // network::TestNetworkContext:
  void CreateTCPConnectedSocket(
      const absl::optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      network::mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::TCPConnectedSocket> socket,
      mojo::PendingRemote<network::mojom::SocketObserver> observer,
      CreateTCPConnectedSocketCallback callback) override {
    const net::IPEndPoint& peer_addr = remote_addr_list.front();
    Record(RecordedCall{
        DirectSocketsServiceImpl::ProtocolType::kTcp,
        peer_addr.address().ToString(), peer_addr.port(),
        tcp_connected_socket_options->send_buffer_size,
        tcp_connected_socket_options->receive_buffer_size,
        tcp_connected_socket_options->no_delay,
        std::move(tcp_connected_socket_options->keep_alive_options)});

    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    MojoResult result = mojo::CreateDataPipe(nullptr, producer, consumer);
    DCHECK_EQ(MOJO_RESULT_OK, result);
    std::move(callback).Run(
        result_, net::IPEndPoint{net::IPAddress::IPv4Localhost(), 0}, peer_addr,
        std::move(consumer), std::move(producer));
  }

 private:
  std::unique_ptr<content::test::MockUDPSocket> CreateMockUDPSocket(
      mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener) override;

  const net::Error result_;
  std::vector<RecordedCall> history_;
};

class MockOpenUDPSocket : public content::test::MockUDPSocket {
 public:
  MockOpenUDPSocket(
      MockOpenNetworkContext* network_context,
      mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener)
      : MockUDPSocket(std::move(receiver), std::move(listener)),
        network_context_(network_context) {}

  ~MockOpenUDPSocket() override = default;

  // network::mojom::UDPSocket:
  void Connect(const net::IPEndPoint& remote_addr,
               network::mojom::UDPSocketOptionsPtr socket_options,
               ConnectCallback callback) override {
    const net::Error result = (remote_addr.port() == 0)
                                  ? net::ERR_INVALID_ARGUMENT
                                  : network_context_->result();
    network_context_->Record(
        RecordedCall{DirectSocketsServiceImpl::ProtocolType::kUdp,
                     remote_addr.address().ToString(),
                     remote_addr.port(),
                     socket_options->send_buffer_size,
                     socket_options->receive_buffer_size,
                     {}});

    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), result,
                       net::IPEndPoint{net::IPAddress::IPv4Localhost(), 0}));
  }

 private:
  const raw_ptr<MockOpenNetworkContext> network_context_;
};

std::unique_ptr<content::test::MockUDPSocket>
MockOpenNetworkContext::CreateMockUDPSocket(
    mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener) {
  return std::make_unique<MockOpenUDPSocket>(this, std::move(receiver),
                                             std::move(listener));
}

}  // anonymous namespace

class DirectSocketsOpenBrowserTest : public ContentBrowserTest {
 public:
  ~DirectSocketsOpenBrowserTest() override = default;

  GURL GetTestOpenPageURL() {
    return embedded_test_server()->GetURL("/direct_sockets/open.html");
  }

 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));
  }

  void SetUp() override {
    embedded_test_server()->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(embedded_test_server()->Start());

    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    std::string origin_list = GetTestOpenPageURL().spec();

    command_line->AppendSwitchASCII(switches::kIsolatedAppOrigins, origin_list);
  }
};

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, OpenTcp_Success_Hostname) {
  const char kExampleHostname[] = "mail.example.com";
  const char kExampleAddress[] = "98.76.54.32";
  const std::string mapping_rules =
      base::StringPrintf("MAP %s %s", kExampleHostname, kExampleAddress);

  MockOpenNetworkContext mock_network_context(net::OK);
  mock_network_context.set_host_mapping_rules(mapping_rules);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);
  const std::string expected_result = base::StringPrintf(
      "openTcp succeeded: {remoteAddress: \"%s\", remotePort: 993}",
      kExampleAddress);

  const std::string script = JsReplace("openTcp($1, 993)", kExampleHostname);

  EXPECT_EQ(expected_result, EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest,
                       OpenTcp_KeepAliveOptionsDelayLessThanASecond) {
  const std::string script =
      JsReplace("openTcp($1, 228, { keepAliveDelay: 950 })", kLocalhostAddress);

  EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
              ::testing::HasSubstr("keepAliveDelay must be no less than"));
}

using ProtocolType = DirectSocketsServiceImpl::ProtocolType;

class DirectSocketsOpenCannotConnectBrowserTest
    : public DirectSocketsOpenBrowserTest,
      public testing::WithParamInterface<ProtocolType> {
 public:
  static std::vector<std::string> ProduceAllTestParams() {
    std::vector<std::string> params;
    std::copy(std::begin(kIPv4_tests), std::end(kIPv4_tests),
              std::back_inserter(params));
    std::transform(std::begin(kIPv4_tests), std::end(kIPv4_tests),
                   std::back_inserter(params),
                   [](const std::string& ip_address) {
                     net::IPAddress address;
                     EXPECT_TRUE(address.AssignFromIPLiteral(ip_address));
                     net::IPAddress mapped_address =
                         net::ConvertIPv4ToIPv4MappedIPv6(address);
                     return base::StrCat({"[", mapped_address.ToString(), "]"});
                   });
    std::transform(std::begin(kIPv6_tests), std::end(kIPv6_tests),
                   std::back_inserter(params),
                   [](const std::string& ip_address) {
                     return base::StrCat({"[", ip_address, "]"});
                   });
    return params;
  }

  void RunTest() {
    const auto protocol = GetParam();
    const std::string type =
        protocol == DirectSocketsServiceImpl::ProtocolType::kTcp ? "Tcp"
                                                                 : "Udp";
    const std::string expected_result = base::StringPrintf(
        "open%s failed: NetworkError: Network Error.", type.c_str());

    const std::string example_hostname = "mail.example.com";
    const std::string script =
        protocol == DirectSocketsServiceImpl::ProtocolType::kTcp
            ? base::StringPrintf("openTcp('%s', 993)", example_hostname.c_str())
            : base::StringPrintf(
                  "openUdp({ remoteAddress: '%s', remotePort: 993 })",
                  example_hostname.c_str());

    for (const auto& address : ProduceAllTestParams()) {
      const std::string mapping_rules = base::StringPrintf(
          "MAP %s %s", example_hostname.c_str(), address.c_str());

      MockOpenNetworkContext mock_network_context(net::OK);
      mock_network_context.set_host_mapping_rules(mapping_rules);
      DirectSocketsServiceImpl::SetNetworkContextForTesting(
          &mock_network_context);

      base::HistogramTester histogram_tester;
      histogram_tester.ExpectBucketCount(
          kPermissionDeniedHistogramName,
          blink::mojom::DirectSocketFailureType::kResolvingToNonPublic, 0);

      EXPECT_EQ(expected_result, EvalJs(shell(), script));

      histogram_tester.ExpectBucketCount(
          kPermissionDeniedHistogramName,
          blink::mojom::DirectSocketFailureType::kResolvingToNonPublic, 1);
    }
  }
};

IN_PROC_BROWSER_TEST_P(DirectSocketsOpenCannotConnectBrowserTest,
                       Open_CannotConnectNonPublic) {
  RunTest();
}

INSTANTIATE_TEST_SUITE_P(
    /*empty*/,
    DirectSocketsOpenCannotConnectBrowserTest,
    testing::Values(ProtocolType::kTcp, ProtocolType::kUdp));

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, OpenTcp_OptionsOne) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectUniqueSample(kTCPNetworkFailuresHistogramName,
                                      -net::Error::ERR_PROXY_CONNECTION_FAILED,
                                      0);

  MockOpenNetworkContext mock_network_context(net::ERR_PROXY_CONNECTION_FAILED);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);
  const std::string expected_result =
      "openTcp failed: NetworkError: Network Error.";

  const std::string script =
      R"(
          openTcp(
            '12.34.56.78',
            9012, {
              sendBufferSize: 3456,
              receiveBufferSize: 7890,
              noDelay: false
            }
          )
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
  EXPECT_TRUE(call.keep_alive_options);
  EXPECT_EQ(false, call.keep_alive_options->enable);

  // To sync histograms from renderer.
  FetchHistogramsFromChildProcesses();
  histogram_tester.ExpectUniqueSample(kTCPNetworkFailuresHistogramName,
                                      -net::Error::ERR_PROXY_CONNECTION_FAILED,
                                      1);
}

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, OpenTcp_OptionsTwo) {
  MockOpenNetworkContext mock_network_context(net::OK);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  const std::string script =
      R"(
          openTcp(
            'fedc:ba98:7654:3210:fedc:ba98:7654:3210',
            789, {
              sendBufferSize: 1243,
              receiveBufferSize: 1234,
              noDelay: true,
              keepAliveDelay: 100_000
            }
          )
        )";
  EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
              StartsWith("openTcp succeeded"));

  DCHECK_EQ(1U, mock_network_context.history().size());
  const RecordedCall& call = mock_network_context.history()[0];
  EXPECT_EQ(DirectSocketsServiceImpl::ProtocolType::kTcp, call.protocol_type);
  EXPECT_EQ("fedc:ba98:7654:3210:fedc:ba98:7654:3210", call.remote_address);
  EXPECT_EQ(789, call.remote_port);
  EXPECT_EQ(1243, call.send_buffer_size);
  EXPECT_EQ(1234, call.receive_buffer_size);
  EXPECT_EQ(true, call.no_delay);
  EXPECT_TRUE(call.keep_alive_options);
  EXPECT_EQ(true, call.keep_alive_options->enable);
  EXPECT_EQ(100U, call.keep_alive_options->delay);
}

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, OpenTcp_OptionsThree) {
  MockOpenNetworkContext mock_network_context(net::OK);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  const std::string script =
      R"(
          openTcp(
            'fedc:ba98:7654:3210:fedc:ba98:7654:3210',
            789, {
              sendBufferSize: 1243,
              receiveBufferSize: 1234,
              noDelay: true,
            }
          )
        )";
  EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
              StartsWith("openTcp succeeded"));

  ASSERT_EQ(1U, mock_network_context.history().size());
  const RecordedCall& call = mock_network_context.history()[0];
  EXPECT_EQ(DirectSocketsServiceImpl::ProtocolType::kTcp, call.protocol_type);
  EXPECT_EQ("fedc:ba98:7654:3210:fedc:ba98:7654:3210", call.remote_address);
  EXPECT_EQ(789, call.remote_port);
  EXPECT_EQ(1243, call.send_buffer_size);
  EXPECT_EQ(1234, call.receive_buffer_size);
  EXPECT_EQ(true, call.no_delay);
  EXPECT_TRUE(call.keep_alive_options);
  EXPECT_EQ(false, call.keep_alive_options->enable);
}

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, OpenUdp_Success_Hostname) {
  const char kExampleHostname[] = "mail.example.com";
  const char kExampleAddress[] = "98.76.54.32";
  const std::string mapping_rules =
      base::StringPrintf("MAP %s %s", kExampleHostname, kExampleAddress);

  MockOpenNetworkContext mock_network_context(net::OK);
  mock_network_context.set_host_mapping_rules(mapping_rules);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);
  const std::string expected_result = base::StringPrintf(
      "openUdp succeeded: {remoteAddress: \"%s\", remotePort: 993}",
      kExampleAddress);

  const std::string script = JsReplace(
      "openUdp({ remoteAddress: $1, remotePort: 993 })", kExampleHostname);

  EXPECT_EQ(expected_result, EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, OpenUdp_NotAllowedError) {
  MockOpenNetworkContext mock_network_context(net::OK);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  // Port 0 is not permitted by MockUDPSocket.
  const std::string script = JsReplace(
      "openUdp({ remoteAddress: $1, remotePort: $2 })", kLocalhostAddress, 0);

  EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
              ::testing::HasSubstr("NetworkError"));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, OpenUdp_OptionsOne) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectUniqueSample(kUDPNetworkFailuresHistogramName,
                                      -net::Error::ERR_PROXY_CONNECTION_FAILED,
                                      0);

  MockOpenNetworkContext mock_network_context(net::ERR_PROXY_CONNECTION_FAILED);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);
  const std::string expected_result =
      "openUdp failed: NetworkError: Network Error.";

  const std::string script = R"(
    openUdp({
      remoteAddress: '12.34.56.78',
      remotePort: 9012,
      sendBufferSize: 3456,
      receiveBufferSize: 7890
    })
  )";
  EXPECT_EQ(expected_result, EvalJs(shell(), script));

  ASSERT_EQ(1U, mock_network_context.history().size());
  const RecordedCall& call = mock_network_context.history()[0];
  EXPECT_EQ(DirectSocketsServiceImpl::ProtocolType::kUdp, call.protocol_type);
  EXPECT_EQ("12.34.56.78", call.remote_address);
  EXPECT_EQ(9012, call.remote_port);
  EXPECT_EQ(3456, call.send_buffer_size);
  EXPECT_EQ(7890, call.receive_buffer_size);

  // To sync histograms from renderer.
  FetchHistogramsFromChildProcesses();
  histogram_tester.ExpectUniqueSample(kUDPNetworkFailuresHistogramName,
                                      -net::Error::ERR_PROXY_CONNECTION_FAILED,
                                      1);
}

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, OpenUdp_OptionsTwo) {
  MockOpenNetworkContext mock_network_context(net::OK);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  const std::string script = R"(
    openUdp({
      remoteAddress: 'fedc:ba98:7654:3210:fedc:ba98:7654:3210',
      remotePort: 789,
      sendBufferSize: 1243,
      receiveBufferSize: 1234
    })
  )";
  EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
              StartsWith("openUdp succeeded"));

  DCHECK_EQ(1U, mock_network_context.history().size());
  const RecordedCall& call = mock_network_context.history()[0];
  EXPECT_EQ(DirectSocketsServiceImpl::ProtocolType::kUdp, call.protocol_type);
  EXPECT_EQ("fedc:ba98:7654:3210:fedc:ba98:7654:3210", call.remote_address);
  EXPECT_EQ(789, call.remote_port);
  EXPECT_EQ(1243, call.send_buffer_size);
  EXPECT_EQ(1234, call.receive_buffer_size);
}

class DirectSocketsOpenCorsBrowserTest
    : public DirectSocketsOpenBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  DirectSocketsOpenCorsBrowserTest()
      : https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUp() override {
    https_server()->RegisterDefaultHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/",
        base::BindRepeating(
            &DirectSocketsOpenCorsBrowserTest::HandleCORSRequest,
            base::Unretained(this), GetParam())));
    ASSERT_TRUE(https_server()->Start(4344));
    DirectSocketsOpenBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(switches::kIsolatedAppOrigins,
                                    GetTestOpenPageURL().spec());
  }

 protected:
  std::unique_ptr<net::test_server::HttpResponse> HandleCORSRequest(
      bool cors_success,
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();

    if (request.method == net::test_server::METHOD_OPTIONS) {
      if (cors_success) {
        response->AddCustomHeader(
            network::cors::header_names::kAccessControlAllowOrigin, "*");

        response->AddCustomHeader(
            network::cors::header_names::kAccessControlAllowHeaders, "*");
      }
    } else {
      response->AddCustomHeader(
          network::cors::header_names::kAccessControlAllowOrigin, "*");
      response->set_content("OK");
    }

    return response;
  }

  net::test_server::EmbeddedTestServer* https_server() {
    return &https_server_;
  }

 private:
  net::test_server::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_P(DirectSocketsOpenCorsBrowserTest, OpenTcp) {
  MockOpenNetworkContext mock_network_context(net::OK);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);
  // HTTPS uses port 443. We cannot really start a server on port 443,
  // therefore we mock the behavior.
  ResolveHostAndOpenSocket::SetHttpsPortForTesting(https_server()->port());

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      blink::mojom::DirectSocketFailureType::kCORS, 0);

  const std::string script =
      JsReplace("openTcp($1, $2)", kLocalhostAddress, https_server()->port());

  bool cors_success = GetParam();

  auto script_result = EvalJs(shell(), script).ExtractString();
  if (cors_success) {
    EXPECT_THAT(script_result, ::testing::HasSubstr("openTcp succeeded"));
  } else {
    EXPECT_THAT(
        script_result,
        ::testing::AllOf(::testing::HasSubstr("InvalidAccessError"),
                         ::testing::HasSubstr("blocked by cross-origin")));
  }

  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      blink::mojom::DirectSocketFailureType::kCORS, cors_success ? 0 : 1);
}

IN_PROC_BROWSER_TEST_P(DirectSocketsOpenCorsBrowserTest, OpenUdp) {
  MockOpenNetworkContext mock_network_context(net::OK);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  // HTTPS uses port 443. We cannot really start a server on port 443,
  // therefore we mock the behavior.
  ResolveHostAndOpenSocket::SetHttpsPortForTesting(https_server()->port());

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      blink::mojom::DirectSocketFailureType::kCORS, 0);

  const std::string script =
      JsReplace("openUdp({ remoteAddress: $1, remotePort: $2 })",
                kLocalhostAddress, https_server()->port());

  bool cors_success = GetParam();

  auto script_result = EvalJs(shell(), script).ExtractString();
  if (cors_success) {
    EXPECT_THAT(script_result, ::testing::HasSubstr("openUdp succeeded"));
  } else {
    EXPECT_THAT(
        script_result,
        ::testing::AllOf(::testing::HasSubstr("InvalidAccessError"),
                         ::testing::HasSubstr("blocked by cross-origin")));
  }

  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      blink::mojom::DirectSocketFailureType::kCORS, cors_success ? 0 : 1);
}

INSTANTIATE_TEST_SUITE_P(/*no prefix*/,
                         DirectSocketsOpenCorsBrowserTest,
                         testing::Bool());

}  // namespace content
