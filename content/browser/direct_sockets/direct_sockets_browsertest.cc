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
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
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
#include "net/net_buildflags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "url/gurl.h"

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
        base::nullopt);
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
              mojo::CreateDataPipe(nullptr, producer, consumer));
    std::move(callback).Run(result_, local_addr, peer_addr, std::move(consumer),
                            std::move(producer));
  }

  void CreateHostResolver(
      const base::Optional<net::DnsConfigOverrides>& config_overrides,
      mojo::PendingReceiver<network::mojom::HostResolver> receiver) override {
    DCHECK(!config_overrides.has_value());
    DCHECK(!internal_resolver_);
    DCHECK(!host_resolver_);

    internal_resolver_ = net::HostResolver::CreateStandaloneResolver(
        net::NetLog::Get(), /*options=*/base::nullopt, host_mapping_rules_,
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
};

net::Error UnconditionallyPermitConnection(
    const blink::mojom::DirectSocketOptions& options) {
  DCHECK(options.remote_hostname.has_value());
  return net::OK;
}

class ReadWriteWaiter {
 public:
  ReadWriteWaiter(
      uint32_t required_receive_bytes,
      uint32_t required_send_bytes,
      mojo::Remote<network::mojom::TCPServerSocket>& tcp_server_socket)
      : required_receive_bytes_(required_receive_bytes),
        required_send_bytes_(required_send_bytes) {
    tcp_server_socket->Accept(
        /*observer=*/mojo::NullRemote(),
        base::BindRepeating(&ReadWriteWaiter::OnAccept,
                            base::Unretained(this)));
  }

  void Await() { run_loop_.Run(); }

 private:
  void OnAccept(
      int result,
      const base::Optional<net::IPEndPoint>& remote_addr,
      mojo::PendingRemote<network::mojom::TCPConnectedSocket> accepted_socket,
      mojo::ScopedDataPipeConsumerHandle consumer_handle,
      mojo::ScopedDataPipeProducerHandle producer_handle) {
    DCHECK_EQ(result, net::OK);
    DCHECK(!accepted_socket_);
    accepted_socket_.Bind(std::move(accepted_socket));

    if (required_receive_bytes_ > 0) {
      receive_stream_ = std::move(consumer_handle);
      read_watcher_ = std::make_unique<mojo::SimpleWatcher>(
          FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);
      read_watcher_->Watch(
          receive_stream_.get(),
          MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
          MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
          base::BindRepeating(&ReadWriteWaiter::OnReadReady,
                              base::Unretained(this)));
      read_watcher_->ArmOrNotify();
    }

    if (required_send_bytes_ > 0) {
      send_stream_ = std::move(producer_handle);
      write_watcher_ = std::make_unique<mojo::SimpleWatcher>(
          FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);
      write_watcher_->Watch(
          send_stream_.get(),
          MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
          MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
          base::BindRepeating(&ReadWriteWaiter::OnWriteReady,
                              base::Unretained(this)));
      write_watcher_->ArmOrNotify();
    }
  }

  void OnReadReady(MojoResult result, const mojo::HandleSignalsState& state) {
    ReadData();
  }

  void OnWriteReady(MojoResult result, const mojo::HandleSignalsState& state) {
    WriteData();
  }

  void ReadData() {
    while (true) {
      DCHECK(receive_stream_.is_valid());
      DCHECK_LT(bytes_received_, required_receive_bytes_);
      const void* buffer = nullptr;
      uint32_t num_bytes = 0;
      MojoResult mojo_result = receive_stream_->BeginReadData(
          &buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
      if (mojo_result == MOJO_RESULT_SHOULD_WAIT) {
        read_watcher_->ArmOrNotify();
        return;
      }
      DCHECK_EQ(mojo_result, MOJO_RESULT_OK);

      // This is guaranteed by Mojo.
      DCHECK_GT(num_bytes, 0u);

      const unsigned char* current = static_cast<const unsigned char*>(buffer);
      const unsigned char* const end = current + num_bytes;
      while (current < end) {
        EXPECT_EQ(*current, bytes_received_ % 256);
        ++current;
        ++bytes_received_;
      }

      mojo_result = receive_stream_->EndReadData(num_bytes);
      DCHECK_EQ(mojo_result, MOJO_RESULT_OK);

      if (bytes_received_ == required_receive_bytes_) {
        if (bytes_sent_ == required_send_bytes_)
          run_loop_.Quit();
        return;
      }
    }
  }

  void WriteData() {
    while (true) {
      DCHECK(send_stream_.is_valid());
      DCHECK_LT(bytes_sent_, required_send_bytes_);
      void* buffer = nullptr;
      uint32_t num_bytes = 0;
      MojoResult mojo_result = send_stream_->BeginWriteData(
          &buffer, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
      if (mojo_result == MOJO_RESULT_SHOULD_WAIT) {
        write_watcher_->ArmOrNotify();
        return;
      }
      DCHECK_EQ(mojo_result, MOJO_RESULT_OK);

      // This is guaranteed by Mojo.
      DCHECK_GT(num_bytes, 0u);

      num_bytes = std::min(num_bytes, required_send_bytes_ - bytes_sent_);

      unsigned char* current = static_cast<unsigned char*>(buffer);
      unsigned char* const end = current + num_bytes;
      while (current != end) {
        *current = bytes_sent_ % 256;
        ++current;
        ++bytes_sent_;
      }

      mojo_result = send_stream_->EndWriteData(num_bytes);
      DCHECK_EQ(mojo_result, MOJO_RESULT_OK);

      if (bytes_sent_ == required_send_bytes_) {
        if (bytes_received_ == required_receive_bytes_)
          run_loop_.Quit();
        return;
      }
    }
  }

  const uint32_t required_receive_bytes_;
  const uint32_t required_send_bytes_;
  base::RunLoop run_loop_;
  mojo::Remote<network::mojom::TCPConnectedSocket> accepted_socket_;
  mojo::ScopedDataPipeConsumerHandle receive_stream_;
  mojo::ScopedDataPipeProducerHandle send_stream_;
  std::unique_ptr<mojo::SimpleWatcher> read_watcher_;
  std::unique_ptr<mojo::SimpleWatcher> write_watcher_;
  uint32_t bytes_received_ = 0;
  uint32_t bytes_sent_ = 0;
};

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

  std::string CreateMDNSHostName() {
    DCHECK(!mdns_responder_.is_bound());
    GetNetworkContext()->CreateMdnsResponder(
        mdns_responder_.BindNewPipeAndPassReceiver());

    std::string name;
    base::RunLoop run_loop;
    mdns_responder_->CreateNameForAddress(
        net::IPAddress::IPv4Localhost(),
        base::BindLambdaForTesting(
            [&name, &run_loop](const std::string& name_out,
                               bool announcement_scheduled) {
              name = name_out;
              run_loop.Quit();
            }));
    run_loop.Run();
    return name;
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

  mojo::Remote<network::mojom::TCPServerSocket>& tcp_server_socket() {
    return tcp_server_socket_;
  }

  void IPRoutableTest(const std::string& address,
                      const DirectSocketsServiceImpl::ProtocolType protocol) {
    EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

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
    DirectSocketsServiceImpl::SetEnterpriseManagedForTesting(false);

    embedded_test_server()->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(embedded_test_server()->Start());

    ContentBrowserTest::SetUp();
  }

 private:
  BrowserContext* browser_context() {
    return shell()->web_contents()->GetBrowserContext();
  }

  base::test::ScopedFeatureList feature_list_;
  mojo::Remote<network::mojom::MdnsResponder> mdns_responder_;
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

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenTcp_MDNS) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  const uint16_t listening_port = StartTcpServer();
  const std::string name = CreateMDNSHostName();
  EXPECT_TRUE(base::EndsWith(name, ".local"));

  const std::string script =
      base::StringPrintf("openTcp({remoteAddress: '%s', remotePort: %d})",
                         name.c_str(), listening_port);

#if BUILDFLAG(ENABLE_MDNS)
  EXPECT_THAT(EvalJs(shell(), script).ExtractString(),
              StartsWith("openTcp succeeded"));
#else
  EXPECT_EQ("openTcp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
#endif  // BUILDFLAG(ENABLE_MDNS)
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenTcp_TransientActivation) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      DirectSocketsServiceImpl::FailureType::kTransientActivation, 0);

  const uint16_t listening_port = StartTcpServer();
  const std::string script = base::StringPrintf(
      "openTcp({remoteAddress: '127.0.0.1', remotePort: %d});\
       openTcp({remoteAddress: '127.0.0.1', remotePort: %d})",
      listening_port, listening_port);

  EXPECT_EQ("openTcp failed: NotAllowedError: Permission denied",
            EvalJs(shell(), script));
  histogram_tester.ExpectBucketCount(
      kPermissionDeniedHistogramName,
      DirectSocketsServiceImpl::FailureType::kTransientActivation, 1);
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenTcp_CannotEvadeCors) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

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

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest,
                       OpenTcp_RestrictedByEnterprisePolicies) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

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

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest,
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

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, WriteTcp) {
  const uint32_t kRequiredBytes = 10000;
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  const uint16_t listening_port = StartTcpServer();
  ReadWriteWaiter waiter(/*required_receive_bytes=*/kRequiredBytes,
                         /*required_send_bytes=*/0, tcp_server_socket());

  const std::string script = base::StringPrintf(
      "writeTcp({remoteAddress: '127.0.0.1', remotePort: %d}, %u)",
      listening_port, kRequiredBytes);
  EXPECT_EQ("write succeeded", EvalJs(shell(), script));
  waiter.Await();
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, ReadTcp) {
  const uint32_t kRequiredBytes = 150000;
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  const uint16_t listening_port = StartTcpServer();
  ReadWriteWaiter waiter(/*required_receive_bytes=*/0,
                         /*required_send_bytes=*/kRequiredBytes,
                         tcp_server_socket());

  const std::string script = base::StringPrintf(
      "readTcp({remoteAddress: '127.0.0.1', remotePort: %d}, %u)",
      listening_port, kRequiredBytes);
  EXPECT_EQ("read succeeded", EvalJs(shell(), script));
  waiter.Await();
}

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, ReadWriteTcp) {
  const uint32_t kRequiredBytes = 1000;
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

  const uint16_t listening_port = StartTcpServer();
  ReadWriteWaiter waiter(/*required_receive_bytes=*/kRequiredBytes,
                         /*required_send_bytes=*/kRequiredBytes,
                         tcp_server_socket());

  const std::string script = base::StringPrintf(
      "readWriteTcp({remoteAddress: '127.0.0.1', remotePort: %d}, %u)",
      listening_port, kRequiredBytes);
  EXPECT_EQ("readWrite succeeded", EvalJs(shell(), script));
  waiter.Await();
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

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest, OpenUdp_TransientActivation) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

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

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest,
                       OpenUdp_RestrictedByEnterprisePolicies) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestPageURL()));

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

IN_PROC_BROWSER_TEST_F(DirectSocketsBrowserTest,
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

}  // namespace content
