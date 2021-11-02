// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/direct_sockets/direct_sockets_service_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
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
#include "net/dns/host_resolver.h"
#include "net/net_buildflags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/test/test_network_context.h"
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
};

constexpr char kPermissionDeniedHistogramName[] =
    "DirectSockets.PermissionDeniedFailures";

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

class MockHostResolver : public network::mojom::HostResolver {
 public:
  explicit MockHostResolver(
      mojo::PendingReceiver<network::mojom::HostResolver> resolver_receiver,
      net::HostResolver* internal_resolver)
      : receiver_(this), internal_resolver_(internal_resolver) {
    receiver_.Bind(std::move(resolver_receiver));
  }

  MockHostResolver(const MockHostResolver&) = delete;
  MockHostResolver& operator=(const MockHostResolver&) = delete;

  void ResolveHost(const ::net::HostPortPair& host,
                   const ::net::NetworkIsolationKey& network_isolation_key,
                   network::mojom::ResolveHostParametersPtr optional_parameters,
                   ::mojo::PendingRemote<network::mojom::ResolveHostClient>
                       pending_response_client) override {
    DCHECK(!internal_request_);
    DCHECK(!response_client_.is_bound());

    internal_request_ = internal_resolver_->CreateRequest(
        host, network_isolation_key,
        net::NetLogWithSource::Make(net::NetLog::Get(),
                                    net::NetLogSourceType::NONE),
        absl::nullopt);
    mojo::Remote<network::mojom::ResolveHostClient> response_client(
        std::move(pending_response_client));

    int rv = internal_request_->Start(
        base::BindOnce(&MockHostResolver::OnComplete, base::Unretained(this)));
    if (rv != net::ERR_IO_PENDING) {
      response_client->OnComplete(rv, internal_request_->GetResolveErrorInfo(),
                                  internal_request_->GetAddressResults());
      return;
    }

    response_client_ = std::move(response_client);
  }

  void MdnsListen(
      const ::net::HostPortPair& host,
      ::net::DnsQueryType query_type,
      ::mojo::PendingRemote<network::mojom::MdnsListenClient> response_client,
      MdnsListenCallback callback) override {
    NOTIMPLEMENTED();
  }

 private:
  void OnComplete(int error) {
    DCHECK(response_client_.is_bound());
    DCHECK(internal_request_);

    response_client_->OnComplete(error,
                                 internal_request_->GetResolveErrorInfo(),
                                 internal_request_->GetAddressResults());
    response_client_.reset();
  }

  std::unique_ptr<net::HostResolver::ResolveHostRequest> internal_request_;
  mojo::Remote<network::mojom::ResolveHostClient> response_client_;
  mojo::Receiver<network::mojom::HostResolver> receiver_;
  net::HostResolver* const internal_resolver_;
};

class MockNetworkContext;

std::unique_ptr<network::mojom::UDPSocket> CreateMockUDPSocket(
    MockNetworkContext* network_context,
    mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener);

class MockNetworkContext : public network::TestNetworkContext {
 public:
  explicit MockNetworkContext(net::Error result) : result_(result) {}

  MockNetworkContext(const MockNetworkContext&) = delete;
  MockNetworkContext& operator=(const MockNetworkContext&) = delete;

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
    Record(RecordedCall{DirectSocketsServiceImpl::ProtocolType::kTcp,
                        peer_addr.address().ToString(), peer_addr.port(),
                        tcp_connected_socket_options->send_buffer_size,
                        tcp_connected_socket_options->receive_buffer_size,
                        tcp_connected_socket_options->no_delay});

    mojo::ScopedDataPipeProducerHandle producer;
    mojo::ScopedDataPipeConsumerHandle consumer;
    DCHECK_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(nullptr, producer, consumer));
    std::move(callback).Run(result_, local_addr, peer_addr, std::move(consumer),
                            std::move(producer));
  }

  void CreateUDPSocket(
      mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
      mojo::PendingRemote<network::mojom::UDPSocketListener> listener)
      override {
    udp_socket_ =
        CreateMockUDPSocket(this, std::move(receiver), std::move(listener));
  }

  void CreateHostResolver(
      const absl::optional<net::DnsConfigOverrides>& config_overrides,
      mojo::PendingReceiver<network::mojom::HostResolver> receiver) override {
    DCHECK(!config_overrides.has_value());
    DCHECK(!internal_resolver_);
    DCHECK(!host_resolver_);

    internal_resolver_ = net::HostResolver::CreateStandaloneResolver(
        net::NetLog::Get(), /*options=*/absl::nullopt, host_mapping_rules_,
        /*enable_caching=*/false);
    host_resolver_ = std::make_unique<MockHostResolver>(
        std::move(receiver), internal_resolver_.get());
  }

  // If set to non-empty, the mapping rules will be applied to requests to the
  // created internal host resolver. See MappedHostResolver for details. Should
  // be called before CreateHostResolver().
  void set_host_mapping_rules(std::string host_mapping_rules) {
    DCHECK(!internal_resolver_);
    host_mapping_rules_ = std::move(host_mapping_rules);
  }

 private:
  const net::Error result_;
  std::vector<RecordedCall> history_;
  std::string host_mapping_rules_;
  std::unique_ptr<net::HostResolver> internal_resolver_;
  std::unique_ptr<network::mojom::HostResolver> host_resolver_;
  std::unique_ptr<network::mojom::UDPSocket> udp_socket_;
};

class MockUDPSocket : public network::mojom::UDPSocket {
 public:
  typedef net::IPAddress IPAddress;
  typedef net::IPEndPoint IPEndPoint;
  typedef net::MutableNetworkTrafficAnnotationTag
      MutableNetworkTrafficAnnotationTag;

  MockUDPSocket(MockNetworkContext* network_context,
                mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
                mojo::PendingRemote<network::mojom::UDPSocketListener> listener)
      : network_context_(network_context) {
    receiver_.Bind(std::move(receiver));
    listener_.Bind(std::move(listener));
  }

  ~MockUDPSocket() override = default;

  // network::mojom::UDPSocket:
  void Bind(const IPEndPoint& local_addr,
            network::mojom::UDPSocketOptionsPtr options,
            BindCallback callback) override {
    NOTIMPLEMENTED();
  }

  void Connect(const IPEndPoint& remote_addr,
               network::mojom::UDPSocketOptionsPtr socket_options,
               ConnectCallback callback) override {
    const net::Error result = (remote_addr.port() == 0)
                                  ? net::ERR_INVALID_ARGUMENT
                                  : network_context_->result();
    network_context_->Record(RecordedCall{
        DirectSocketsServiceImpl::ProtocolType::kUdp,
        remote_addr.address().ToString(), remote_addr.port(),
        socket_options->send_buffer_size, socket_options->receive_buffer_size,
        /*no_delay=*/false});

    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), result,
                                  /*local_addr_out=*/absl::nullopt));
  }

  void SetBroadcast(bool broadcast, SetBroadcastCallback callback) override {
    NOTIMPLEMENTED();
  }

  void SetSendBufferSize(int32_t send_buffer_size,
                         SetSendBufferSizeCallback callback) override {
    NOTIMPLEMENTED();
  }

  void SetReceiveBufferSize(int32_t receive_buffer_size,
                            SetSendBufferSizeCallback callback) override {
    NOTIMPLEMENTED();
  }

  void JoinGroup(const IPAddress& group_address,
                 JoinGroupCallback callback) override {
    NOTIMPLEMENTED();
  }

  void LeaveGroup(const IPAddress& group_address,
                  LeaveGroupCallback callback) override {
    NOTIMPLEMENTED();
  }

  void ReceiveMore(uint32_t num_additional_datagrams) override {
    NOTIMPLEMENTED();
  }

  void ReceiveMoreWithBufferSize(uint32_t num_additional_datagrams,
                                 uint32_t buffer_size) override {
    NOTIMPLEMENTED();
  }

  void SendTo(const IPEndPoint& dest_addr,
              base::span<const uint8_t> data,
              const MutableNetworkTrafficAnnotationTag& traffic_annotation,
              SendToCallback callback) override {
    NOTIMPLEMENTED();
  }

  void Send(base::span<const uint8_t> data,
            const MutableNetworkTrafficAnnotationTag& traffic_annotation,
            SendCallback callback) override {
    NOTIMPLEMENTED();
  }

  void Close() override { NOTIMPLEMENTED(); }

  MockNetworkContext* const network_context_;
  mojo::Receiver<network::mojom::UDPSocket> receiver_{this};
  mojo::Remote<network::mojom::UDPSocketListener> listener_;
};

std::unique_ptr<network::mojom::UDPSocket> CreateMockUDPSocket(
    MockNetworkContext* network_context,
    mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener) {
  return std::make_unique<MockUDPSocket>(network_context, std::move(receiver),
                                         std::move(listener));
}

net::Error UnconditionallyPermitConnection(
    const blink::mojom::DirectSocketOptions& options) {
  DCHECK(options.remote_hostname.has_value());
  return net::OK;
}

}  // anonymous namespace

class DirectSocketsOpenBrowserTest : public ContentBrowserTest {
 public:
  DirectSocketsOpenBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kDirectSockets);
  }
  ~DirectSocketsOpenBrowserTest() override = default;

  GURL GetTestOpenPageURL() {
    return embedded_test_server()->GetURL("/direct_sockets/open.html");
  }

  void IPRoutableTest(const std::string& address,
                      const DirectSocketsServiceImpl::ProtocolType protocol) {
    EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

    const char kExampleHostname[] = "mail.example.com";
    const std::string mapping_rules =
        base::StringPrintf("MAP %s %s", kExampleHostname, address.c_str());

    MockNetworkContext mock_network_context(net::OK);
    mock_network_context.set_host_mapping_rules(mapping_rules);
    DirectSocketsServiceImpl::SetNetworkContextForTesting(
        &mock_network_context);
    const std::string type =
        protocol == DirectSocketsServiceImpl::ProtocolType::kTcp ? "Tcp"
                                                                 : "Udp";
    const std::string expected_result = base::StringPrintf(
        "open%s failed: NotAllowedError: Permission denied", type.c_str());

    base::HistogramTester histogram_tester;
    histogram_tester.ExpectBucketCount(
        kPermissionDeniedHistogramName,
        DirectSocketsServiceImpl::FailureType::kResolvingToNonPublic, 0);

    const std::string script =
        base::StringPrintf("open%s({remoteAddress: '%s', remotePort: 993})",
                           type.c_str(), kExampleHostname);

    EXPECT_EQ(expected_result, EvalJs(shell(), script));
    histogram_tester.ExpectBucketCount(
        kPermissionDeniedHistogramName,
        DirectSocketsServiceImpl::FailureType::kResolvingToNonPublic, 1);
  }

 protected:
  void SetUp() override {
    DirectSocketsServiceImpl::SetConnectionDialogBypassForTesting(true);
    DirectSocketsServiceImpl::SetEnterpriseManagedForTesting(false);

    embedded_test_server()->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(embedded_test_server()->Start());

    ContentBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, OpenTcp_Success_Hostname) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  const char kExampleHostname[] = "mail.example.com";
  const char kExampleAddress[] = "98.76.54.32";
  const std::string mapping_rules =
      base::StringPrintf("MAP %s %s", kExampleHostname, kExampleAddress);

  MockNetworkContext mock_network_context(net::OK);
  mock_network_context.set_host_mapping_rules(mapping_rules);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);
  const std::string expected_result = base::StringPrintf(
      "openTcp succeeded: {remoteAddress: \"%s\", remotePort: 993}",
      kExampleAddress);

  const std::string script = base::StringPrintf(
      "openTcp({remoteAddress: '%s', remotePort: 993})", kExampleHostname);

  EXPECT_EQ(expected_result, EvalJs(shell(), script));
}

// TODO(crbug.com/1196515): Fix this flaky test.
IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest,
                       DISABLED_OpenTcp_TransientActivation) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      DirectSocketsServiceImpl::FailureType::kTransientActivation, 0);

  MockNetworkContext mock_network_context(net::OK);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  const std::string script =
      "openTcp({remoteAddress: '::1', remotePort: 993});\
       openTcp({remoteAddress: '::1', remotePort: 993})";

  EXPECT_EQ("openTcp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      DirectSocketsServiceImpl::FailureType::kTransientActivation, 1);
}

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, OpenTcp_CannotEvadeCors) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      DirectSocketsServiceImpl::FailureType::kCORS, 0);

  // HTTPS uses port 443.
  const std::string script =
      "openTcp({remoteAddress: '127.0.0.1', remotePort: 443})";

  EXPECT_EQ("openTcp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      DirectSocketsServiceImpl::FailureType::kCORS, 1);
}

// Permission Denied failures(user dialog) should be triggered if connection
// dialog is not accepted.
IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest,
                       OpenTcp_ConnectionDialogNotAccepted) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      DirectSocketsServiceImpl::FailureType::kUserDialog, 0);

  DirectSocketsServiceImpl::SetConnectionDialogBypassForTesting(false);

  const std::string script =
      "openTcp({remoteAddress: '127.0.0.1', remotePort: 993})";

  EXPECT_EQ("openTcp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      DirectSocketsServiceImpl::FailureType::kUserDialog, 1);
}

// Remote address should be provided or TEST will fail with NotAllowedError. In
// actual use scenario, it can be obtained from the user's input in connection
// dialog.
IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest,
                       OpenTcp_RemoteAddressCurrentlyRequired) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  const std::string script = "openTcp({remotePort: 993})";

  EXPECT_EQ("openTcp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest,
                       OpenTcp_RestrictedByEnterprisePolicies) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      DirectSocketsServiceImpl::FailureType::kEnterprisePolicy, 0);

  DirectSocketsServiceImpl::SetEnterpriseManagedForTesting(true);

  const std::string script =
      "openTcp({remoteAddress: '127.0.0.1', remotePort: 993})";

  EXPECT_EQ("openTcp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      DirectSocketsServiceImpl::FailureType::kEnterprisePolicy, 1);
}

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest,
                       OpenTcp_CannotConnectNonPublic) {
  const auto protocol = DirectSocketsServiceImpl::ProtocolType::kTcp;
  // Tests for the reserved IPv4 ranges. The reserved ranges are tested by
  // checking the first and last address of each range. These tests cover the
  // entire IPv4 address range, as well as this range mapped to IPv6.
  for (const auto& test : kIPv4_tests) {
    IPRoutableTest(test, protocol);

    // Check these IPv4 addresses when mapped to IPv6.
    net::IPAddress address;
    EXPECT_TRUE(address.AssignFromIPLiteral(test));
    net::IPAddress mapped_address = net::ConvertIPv4ToIPv4MappedIPv6(address);
    IPRoutableTest(base::StrCat({"[", mapped_address.ToString(), "]"}),
                   protocol);
  }

  // Tests for the reserved IPv6 ranges. The reserved ranges are tested by
  // checking the first and last address of each range. These tests cover the
  // entire IPv6 address range.
  for (const auto& test : kIPv6_tests)
    IPRoutableTest(base::StrCat({"[", test, "]"}), protocol);
}

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, OpenTcp_OptionsOne) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

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

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, OpenTcp_OptionsTwo) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

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

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, OpenUdp_Success) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  MockNetworkContext mock_network_context(net::OK);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  uint16_t remotePort = 513;
  const std::string script = base::StringPrintf(
      "openUdp({remoteAddress: '127.0.0.1', remotePort: %d})", remotePort);

  EXPECT_EQ("openUdp succeeded", EvalJs(shell(), script));
}

// TODO(crbug.com/1213100): Fix this flaky test.
IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest,
                       DISABLED_OpenUdp_TransientActivation) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      DirectSocketsServiceImpl::FailureType::kTransientActivation, 0);

  const std::string script = base::StringPrintf(
      "openUdp({remoteAddress: '127.0.0.1', remotePort: %d});\
       openUdp({remoteAddress: '127.0.0.1', remotePort: %d})",
      0, 0);

  EXPECT_EQ("openUdp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      DirectSocketsServiceImpl::FailureType::kTransientActivation, 1);
}

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, OpenUdp_NotAllowedError) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  MockNetworkContext mock_network_context(net::OK);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  // Port 0 is not permitted by MockUDPSocket.
  const std::string script = base::StringPrintf(
      "openUdp({remoteAddress: '127.0.0.1', remotePort: %d})", 0);

  EXPECT_EQ("openUdp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, OpenUdp_CannotEvadeCors) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      DirectSocketsServiceImpl::FailureType::kCORS, 0);

  // QUIC uses port 443.
  const std::string script =
      "openUdp({remoteAddress: '127.0.0.1', remotePort: 443})";

  EXPECT_EQ("openUdp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      DirectSocketsServiceImpl::FailureType::kCORS, 1);
}

// Permission Denied failures(user dialog) should be triggered if connection
// dialog is not accepted.
IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest,
                       OpenUdp_ConnectionDialogNotAccepted) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      DirectSocketsServiceImpl::FailureType::kUserDialog, 0);

  DirectSocketsServiceImpl::SetConnectionDialogBypassForTesting(false);

  const std::string script =
      "openUdp({remoteAddress: '127.0.0.1', remotePort: 993})";

  EXPECT_EQ("openUdp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      DirectSocketsServiceImpl::FailureType::kUserDialog, 1);
}

// Remote address should be provided or TEST will fail with NotAllowedError. In
// actual use scenario, it can be obtained from the user's input in connection
// dialog.
IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest,
                       OpenUdp_RemoteAddressCurrentlyRequired) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  const std::string script = "openUdp({remotePort: 993})";

  EXPECT_EQ("openUdp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest,
                       OpenUdp_RestrictedByEnterprisePolicies) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      DirectSocketsServiceImpl::FailureType::kEnterprisePolicy, 0);

  DirectSocketsServiceImpl::SetEnterpriseManagedForTesting(true);

  const std::string script =
      "openUdp({remoteAddress: '127.0.0.1', remotePort: 993})";

  EXPECT_EQ("openUdp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      DirectSocketsServiceImpl::FailureType::kEnterprisePolicy, 1);
}

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest,
                       OpenUdp_CannotConnectNonPublic) {
  const auto protocol = DirectSocketsServiceImpl::ProtocolType::kUdp;
  // Tests for the reserved IPv4 ranges. The reserved ranges are tested by
  // checking the first and last address of each range. These tests cover the
  // entire IPv4 address range, as well as this range mapped to IPv6.
  for (const auto& test : kIPv4_tests) {
    IPRoutableTest(test, protocol);

    // Check these IPv4 addresses when mapped to IPv6.
    net::IPAddress address;
    EXPECT_TRUE(address.AssignFromIPLiteral(test));
    net::IPAddress mapped_address = net::ConvertIPv4ToIPv4MappedIPv6(address);
    IPRoutableTest(base::StrCat({"[", mapped_address.ToString(), "]"}),
                   protocol);
  }

  // Tests for the reserved IPv6 ranges. The reserved ranges are tested by
  // checking the first and last address of each range. These tests cover the
  // entire IPv6 address range.
  for (const auto& test : kIPv6_tests)
    IPRoutableTest(base::StrCat({"[", test, "]"}), protocol);
}

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, OpenUdp_OptionsOne) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  MockNetworkContext mock_network_context(net::ERR_PROXY_CONNECTION_FAILED);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);
  const std::string expected_result =
      "openUdp failed: NotAllowedError: Permission denied";

  const std::string script =
      R"(
          openUdp({
            remoteAddress: '12.34.56.78',
            remotePort: 9012,
            sendBufferSize: 3456,
            receiveBufferSize: 7890
          })
        )";
  EXPECT_EQ(expected_result, EvalJs(shell(), script));

  DCHECK_EQ(1U, mock_network_context.history().size());
  const RecordedCall& call = mock_network_context.history()[0];
  EXPECT_EQ(DirectSocketsServiceImpl::ProtocolType::kUdp, call.protocol_type);
  EXPECT_EQ("12.34.56.78", call.remote_address);
  EXPECT_EQ(9012, call.remote_port);
  EXPECT_EQ(3456, call.send_buffer_size);
  EXPECT_EQ(7890, call.receive_buffer_size);
}

IN_PROC_BROWSER_TEST_F(DirectSocketsOpenBrowserTest, OpenUdp_OptionsTwo) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestOpenPageURL()));

  DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
      base::BindRepeating(&UnconditionallyPermitConnection));

  MockNetworkContext mock_network_context(net::OK);
  DirectSocketsServiceImpl::SetNetworkContextForTesting(&mock_network_context);

  const std::string script =
      R"(
          openUdp({
            remoteAddress: 'fedc:ba98:7654:3210:fedc:ba98:7654:3210',
            remotePort: 789,
            sendBufferSize: 0,
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
  EXPECT_EQ(0, call.send_buffer_size);
  EXPECT_EQ(1234, call.receive_buffer_size);
}

}  // namespace content
