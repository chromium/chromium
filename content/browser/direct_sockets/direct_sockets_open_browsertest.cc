// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/direct_sockets/direct_sockets_service_impl.h"
#include "content/browser/direct_sockets/direct_sockets_test_utils.h"
#include "content/public/browser/browser_context.h"
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
#include "net/net_buildflags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_udp_socket.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom.h"
#include "url/gurl.h"

// The tests in this file use a mock implementation of NetworkContext, to test
// DNS resolving, and the opening of TCP and UDP sockets.

using testing::StartsWith;

namespace content {

namespace {

using ProtocolType = blink::mojom::DirectSocketProtocolType;

struct RecordedCall {
  ProtocolType protocol_type;

  std::string remote_address;
  uint16_t remote_port;

  int32_t send_buffer_size = 0;
  int32_t receive_buffer_size = 0;

  bool no_delay = false;

  network::mojom::TCPKeepAliveOptionsPtr keep_alive_options;
};

constexpr char kLocalhostAddress[] = "127.0.0.1";

constexpr char kTCPNetworkFailuresHistogramName[] =
    "DirectSockets.TCPNetworkFailures";

constexpr char kUDPNetworkFailuresHistogramName[] =
    "DirectSockets.UDPNetworkFailures";

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
        ProtocolType::kTcp, peer_addr.address().ToString(), peer_addr.port(),
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
    network_context_->Record(RecordedCall{ProtocolType::kUdp,
                                          remote_addr.address().ToString(),
                                          remote_addr.port(),
                                          socket_options->send_buffer_size,
                                          socket_options->receive_buffer_size,
                                          {}});

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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

    client_ = std::make_unique<test::IsolatedWebAppContentBrowserClient>(
        url::Origin::Create(GetTestOpenPageURL()));
    scoped_client_ =
        std::make_unique<ScopedContentBrowserClientSetting>(client_.get());

    ASSERT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));
  }

  void SetUp() override {
    embedded_test_server()->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(embedded_test_server()->Start());

    ContentBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_{features::kIsolatedWebApps};

  std::unique_ptr<test::IsolatedWebAppContentBrowserClient> client_;
  std::unique_ptr<ScopedContentBrowserClientSetting> scoped_client_;
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
  EXPECT_EQ(ProtocolType::kTcp, call.protocol_type);
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
  EXPECT_EQ(ProtocolType::kTcp, call.protocol_type);
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
  EXPECT_EQ(ProtocolType::kTcp, call.protocol_type);
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
  EXPECT_EQ(ProtocolType::kUdp, call.protocol_type);
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
  EXPECT_EQ(ProtocolType::kUdp, call.protocol_type);
  EXPECT_EQ("fedc:ba98:7654:3210:fedc:ba98:7654:3210", call.remote_address);
  EXPECT_EQ(789, call.remote_port);
  EXPECT_EQ(1243, call.send_buffer_size);
  EXPECT_EQ(1234, call.receive_buffer_size);
}

}  // namespace content
