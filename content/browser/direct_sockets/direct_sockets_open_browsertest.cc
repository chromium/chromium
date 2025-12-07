// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "content/browser/direct_sockets/direct_sockets_service_impl.h"
#include "content/browser/direct_sockets/direct_sockets_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/direct_sockets_delegate.h"
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
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom.h"
#include "url/gurl.h"

// The tests in this file use a mock implementation of NetworkContext, to test
// DNS resolving, and the opening of TCP and UDP sockets.

using testing::StartsWith;

namespace content {

namespace {

using ProtocolType = DirectSocketsDelegate::ProtocolType;

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
  MockOpenNetworkContext(net::Error result, std::string_view host_mapping_rules)
      : MockNetworkContext(host_mapping_rules), result_(result) {}

  ~MockOpenNetworkContext() override = default;

  void Record(RecordedCall call) { history_.push_back(std::move(call)); }

  net::Error result() const { return result_; }

  const std::vector<RecordedCall>& history() const { return history_; }

  // network::TestNetworkContext:
  void CreateTCPConnectedSocket(
      const std::optional<net::IPEndPoint>& local_addr,
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
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener) override;

  const net::Error result_;
  std::vector<RecordedCall> history_;
};

class MockOpenUDPSocket : public content::test::MockUDPSocket {
 public:
  MockOpenUDPSocket(
      MockOpenNetworkContext* network_context,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener)
      : MockUDPSocket(std::move(listener)), network_context_(network_context) {}

  ~MockOpenUDPSocket() override = default;

  // network::mojom::UDPSocket:
  void Connect(const net::IPEndPoint& remote_addr,
               network::mojom::UDPSocketOptionsPtr socket_options,
               ConnectCallback callback) override {
    const net::Error result = (remote_addr.port() == 0)
                                  ? net::ERR_INVALID_ARGUMENT
                                  : network_context_->result();
    network_context_->Record(RecordedCall{ProtocolType::kConnectedUdp,
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
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener) {
  return std::make_unique<MockOpenUDPSocket>(this, std::move(listener));
}

}  // anonymous namespace

class DirectSocketsOpenBrowserTest : public ContentBrowserTest {
 public:
  GURL GetTestOpenPageURL() {
    return embedded_test_server()->GetURL("/direct_sockets/open.html");
  }

 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    client_ = std::make_unique<test::IsolatedWebAppContentBrowserClient>(
        url::Origin::Create(GetTestOpenPageURL()));

    ASSERT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));
  }

  void SetUp() override {
    embedded_test_server()->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(embedded_test_server()->Start());

    ContentBrowserTest::SetUp();
  }

 private:
  std::unique_ptr<test::IsolatedWebAppContentBrowserClient> client_;
};

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, OpenTcp_Success_Hostname) {
  const char kExampleHostname[] = "mail.example.com";
  const char kExampleAddress[] = "98.76.54.32";
  const std::string mapping_rules =
      base::StringPrintf("MAP %s %s", kExampleHostname, kExampleAddress);

  MockOpenNetworkContext mock_network_context(net::OK, mapping_rules);
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

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest,
                       DISABLED_OpenTcp_OptionsOne) {
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

  MockOpenNetworkContext mock_network_context(net::OK, mapping_rules);
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
  EXPECT_EQ(ProtocolType::kConnectedUdp, call.protocol_type);
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
  EXPECT_EQ(ProtocolType::kConnectedUdp, call.protocol_type);
  EXPECT_EQ("fedc:ba98:7654:3210:fedc:ba98:7654:3210", call.remote_address);
  EXPECT_EQ(789, call.remote_port);
  EXPECT_EQ(1243, call.send_buffer_size);
  EXPECT_EQ(1234, call.receive_buffer_size);
}

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest,
                       OpenUdp_RemoteLocalOptions) {
  {
    const std::string script = R"(
      openUdp({
        remoteAddress: '192.168.0.1',
      })
    )";
    EXPECT_THAT(
        EvalJs(shell(), script).ExtractString(),
        testing::HasSubstr("remoteAddress and remotePort should either"));
  }

  {
    const std::string script = R"(
      openUdp({
        remotePort: 228,
      })
    )";
    EXPECT_THAT(
        EvalJs(shell(), script).ExtractString(),
        testing::HasSubstr("remoteAddress and remotePort should either"));
  }

  {
    const std::string script = R"(
      openUdp({
        remoteAddress: '192.168.0.1',
        remotePort: 228,
        localAddress: '127.0.0.1',
      })
    )";

    EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
                testing::HasSubstr("remoteAddress and localAddress cannot be "
                                   "specified at the same time"));
  }

  {
    const std::string script = R"(
      openUdp({
        localAddress: 'direct-sockets.com',
      })
    )";
    EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
                testing::HasSubstr("localAddress must be a valid IP address"));
  }

  {
    const std::string script = R"(
      openUdp({
        localPort: 228,
      })
    )";
    EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
                testing::HasSubstr(
                    "localPort cannot be specified without localAddress"));
  }

  {
    const std::string script = R"(
      openUdp({})
    )";
    EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
                testing::HasSubstr("neither remoteAddress nor "
                                   "localAddress specified"));
  }

  {
    const std::string script = R"(
      openUdp({
        localAddress: "127.0.0.1",
        dnsQueryType: "ipv4",
      })
    )";
    EXPECT_THAT(
        EvalJs(shell(), script).ExtractString(),
        testing::HasSubstr(
            "dnsQueryType is only relevant when remoteAddress is specified"));
  }
}

class MockOpenNetworkContextWithDnsQueryType : public MockOpenNetworkContext {
 public:
  MockOpenNetworkContextWithDnsQueryType(net::Error result,
                                         std::string_view host_mapping_rules)
      : MockOpenNetworkContext(result, host_mapping_rules) {}

  // MockOpenNetworkContext:
  void ResolveHost(
      network::mojom::HostResolverHostPtr host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      network::mojom::ResolveHostParametersPtr optional_parameters,
      mojo::PendingRemote<network::mojom::ResolveHostClient>
          pending_response_client) override {
    ASSERT_TRUE(expected_dns_query_type_);
    ASSERT_TRUE(optional_parameters);
    ASSERT_EQ(optional_parameters->dns_query_type, expected_dns_query_type_);
    ResolveHostImpl(std::move(host), network_anonymization_key,
                    std::move(optional_parameters),
                    std::move(pending_response_client));
  }

  void set_expected_dns_query_type(
      std::optional<net::DnsQueryType> dns_query_type) {
    expected_dns_query_type_ = std::move(dns_query_type);
  }

 private:
  std::optional<net::DnsQueryType> expected_dns_query_type_;
};

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, Open_DnsQueryType) {
  constexpr std::string_view kHostname = "direct-sockets.com";

  MockOpenNetworkContextWithDnsQueryType mock_network_context(
      net::OK, base::StringPrintf("MAP %s 98.76.54.32", kHostname.data()));
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  constexpr auto kDnsQueryTypeMapping =
      base::MakeFixedFlatMap<net::DnsQueryType, std::string_view>({
          {net::DnsQueryType::A, "ipv4"},
          {net::DnsQueryType::AAAA, "ipv6"},
      });

  for (const auto& [expected_dns_query_type, dns_query_type_str] :
       kDnsQueryTypeMapping) {
    mock_network_context.set_expected_dns_query_type(expected_dns_query_type);
    {
      const std::string script = R"(
        openUdp({
          remoteAddress: $1,
          remotePort: 53,
          dnsQueryType: $2,
        })
      )";
      EXPECT_THAT(
          EvalJs(shell(), JsReplace(script, kHostname, dns_query_type_str))
              .ExtractString(),
          testing::HasSubstr("openUdp succeeded"));
    }
    {
      const std::string script = R"(
        openTcp($1, 53, {
          dnsQueryType: $2,
        })
      )";
      EXPECT_THAT(
          EvalJs(shell(), JsReplace(script, kHostname, dns_query_type_str))
              .ExtractString(),
          testing::HasSubstr("openTcp succeeded"));
    }
  }
}

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, OpenTcpServerOptions) {
  {
    const std::string script = R"(
      openTcpServer('direct-sockets.com');
    )";
    EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
                testing::HasSubstr("localAddress must be a valid IP address"));
  }

  {
    const std::string script = R"(
      openTcpServer('127.0.0.1', {
        localPort: 0,
      });
    )";
    EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
                testing::HasSubstr("localPort must be greater than zero"));
  }

  {
    const std::string script = R"(
      openTcpServer('127.0.0.1', {
        backlog: 0,
      });
    )";
    EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
                testing::HasSubstr("backlog must be greater than zero"));
  }

  {
    const std::string script = R"(
      openTcpServer('127.0.0.1', {
        ipv6Only: true,
      });
    )";
    EXPECT_THAT(
        EvalJs(shell(), script).ExtractString(),
        testing::HasSubstr(
            "ipv6Only can only be specified when localAddress is [::]"));
  }
}

enum class WorkerType { kShared, kService };
enum class FeatureState { kDefault, kDisabled, kEnabled };

template <WorkerType worker_type>
class DirectSocketsWorkerExposureBrowserTest
    : public DirectSocketsOpenBrowserTest,
      public testing::WithParamInterface<FeatureState> {
 public:
  DirectSocketsWorkerExposureBrowserTest() {
    switch (GetFeatureState()) {
      case FeatureState::kDefault:
        break;
      case FeatureState::kEnabled:
        InitWith(&base::test::ScopedFeatureList::InitAndEnableFeature);
        break;
      case FeatureState::kDisabled:
        InitWith(&base::test::ScopedFeatureList::InitAndDisableFeature);
        break;
    }
  }

  void SetUp() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &DirectSocketsWorkerExposureBrowserTest::HandleRequest,
        base::Unretained(this)));
    DirectSocketsOpenBrowserTest::SetUp();
  }

  bool ShouldBeExposed() { return GetFeatureState() == FeatureState::kEnabled; }

 private:
  FeatureState GetFeatureState() { return GetParam(); }

  void InitWith(auto init) {
    switch (worker_type) {
      case WorkerType::kService:
        std::invoke(init, features_,
                    blink::features::kDirectSocketsInServiceWorkers);
        break;
      case WorkerType::kShared:
        std::invoke(init, features_,
                    blink::features::kDirectSocketsInSharedWorkers);
        break;
    }
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url.ends_with("shared_worker.js")) {
      auto response = WorkerScriptResponse();
      response->set_content(R"(
        onconnect = e => {
          const port = e.ports[0];
          port.start();
          port.postMessage({
            'TCPSocket': typeof TCPSocket,
            'UDPSocket': typeof UDPSocket,
            'TCPServerSocket': typeof TCPServerSocket,
          });
        };
      )");
      return response;
    }
    if (request.relative_url.ends_with("service_worker.js")) {
      auto response = WorkerScriptResponse();
      response->set_content(R"(
        addEventListener('message', e => {
          e.source.postMessage({
            'TCPSocket': typeof TCPSocket,
            'UDPSocket': typeof UDPSocket,
            'TCPServerSocket': typeof TCPServerSocket,
          });
        });
      )");
      return response;
    }
    return nullptr;
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> WorkerScriptResponse() {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("application/javascript");
    response->AddCustomHeader("Cross-Origin-Embedder-Policy", "require-corp");
    response->AddCustomHeader("Cross-Origin-Resource-Policy", "same-origin");
    return response;
  }

  base::test::ScopedFeatureList features_;
};

using DirectSocketsSharedWorkerExposureBrowserTest =
    DirectSocketsWorkerExposureBrowserTest<WorkerType::kShared>;

IN_PROC_BROWSER_TEST_P(DirectSocketsSharedWorkerExposureBrowserTest, Exposure) {
  static constexpr char kSharedWorkerStart[] = R"(
    new Promise(resolve => {
      const worker = new SharedWorker('/shared_worker.js');
      worker.port.onmessage = e => resolve(e.data);
    });
  )";

  std::string expected_typeof = ShouldBeExposed() ? "function" : "undefined";
  EXPECT_EQ(EvalJs(shell(), kSharedWorkerStart),
            base::Value::Dict()
                .Set("TCPSocket", expected_typeof)
                .Set("UDPSocket", expected_typeof)
                .Set("TCPServerSocket", expected_typeof));
}

using DirectSocketsServiceWorkerExposureBrowserTest =
    DirectSocketsWorkerExposureBrowserTest<WorkerType::kService>;

IN_PROC_BROWSER_TEST_P(DirectSocketsServiceWorkerExposureBrowserTest,
                       Exposure) {
  static constexpr char kServiceWorkerStart[] = R"(
    new Promise(async (resolve) => {
      await navigator.serviceWorker.register('/service_worker.js');
      navigator.serviceWorker.addEventListener('message', e => resolve(e.data));
      const reg = await navigator.serviceWorker.ready;
      reg.active.postMessage(null);
    });
  )";

  std::string expected_typeof = ShouldBeExposed() ? "function" : "undefined";
  EXPECT_EQ(EvalJs(shell(), kServiceWorkerStart),
            base::Value::Dict()
                .Set("TCPSocket", expected_typeof)
                .Set("UDPSocket", expected_typeof)
                .Set("TCPServerSocket", expected_typeof));
}

INSTANTIATE_TEST_SUITE_P(/**/,
                         DirectSocketsSharedWorkerExposureBrowserTest,
                         testing::Values(FeatureState::kDefault,
                                         FeatureState::kEnabled,
                                         FeatureState::kDisabled));

INSTANTIATE_TEST_SUITE_P(/**/,
                         DirectSocketsServiceWorkerExposureBrowserTest,
                         testing::Values(FeatureState::kDefault,
                                         FeatureState::kEnabled,
                                         FeatureState::kDisabled));

}  // namespace content
