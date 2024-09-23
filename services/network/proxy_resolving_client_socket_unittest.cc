// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_resolving_client_socket.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/base/features.h"
#include "net/base/network_isolation_key.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/base/schemeful_site.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/mock_proxy_resolver.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/connect_job_factory.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/test/gtest_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/proxy_resolving_client_socket_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace network {

namespace {

std::unique_ptr<net::ConfiguredProxyResolutionService>
CreateProxyResolutionService(std::string_view pac_result) {
  return net::ConfiguredProxyResolutionService::CreateFixedFromPacResultForTest(
      static_cast<std::string>(pac_result), TRAFFIC_ANNOTATION_FOR_TESTS);
}

}  // namespace

class ProxyResolvingClientSocketTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 protected:
  ProxyResolvingClientSocketTest() : use_tls_(GetParam()) {
    feature_list_.InitAndEnableFeature(
        net::features::kPartitionConnectionsByNetworkIsolationKey);
  }

  ~ProxyResolvingClientSocketTest() override {}

  void TearDown() override {
    // Clear out any messages posted by ProxyResolvingClientSocket's
    // destructor.
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<net::URLRequestContextBuilder> CreateBuilder(
      std::string_view pac_result = "PROXY bad:99; PROXY maybe:80; DIRECT") {
    auto builder = net::CreateTestURLRequestContextBuilder();
    builder->set_proxy_resolution_service(
        CreateProxyResolutionService(pac_result));
    builder->set_client_socket_factory_for_testing(
        &mock_client_socket_factory_);
    return builder;
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  net::MockClientSocketFactory mock_client_socket_factory_;
  const bool use_tls_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ProxyResolvingClientSocketTest,
                         ::testing::Bool());

// Checks the correct NetworkAnonymizationKey is used for host resolution in the
// case no proxy is in use.
TEST_P(ProxyResolvingClientSocketTest, NetworkIsolationKeyDirect) {
  // This deliberately uses a different origin than the one being connected to.
  const net::SchemefulSite kNetworkIsolationKeySite =
      net::SchemefulSite(GURL("https://foopy.test"));
  const net::NetworkIsolationKey kNetworkIsolationKey(kNetworkIsolationKeySite,
                                                      kNetworkIsolationKeySite);
  const net::NetworkAnonymizationKey kNetworkAnonymizationKey =
      net::NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
          kNetworkIsolationKey);
  auto url_request_context = CreateBuilder("DIRECT")->Build();

  const GURL kDestination("https://dest.test/");
  net::StaticSocketDataProvider socket_data;
  mock_client_socket_factory_.AddSocketDataProvider(&socket_data);
  net::SSLSocketDataProvider ssl_data(net::ASYNC, net::OK);
  mock_client_socket_factory_.AddSSLSocketDataProvider(&ssl_data);

  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      url_request_context.get());
  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(
          kDestination, kNetworkAnonymizationKey, use_tls_);
  net::TestCompletionCallback callback;
  int status = socket->Connect(callback.callback());
  EXPECT_THAT(callback.GetResult(status), net::test::IsOk());

  // Check that the URL in kDestination is in the HostCache, with
  // kNetworkIsolationInfo.
  const net::HostPortPair kDestinationHostPortPair =
      net::HostPortPair::FromURL(kDestination);
  net::HostResolver::ResolveHostParameters params;
  params.source = net::HostResolverSource::LOCAL_ONLY;
  std::unique_ptr<net::HostResolver::ResolveHostRequest> request1 =
      url_request_context->host_resolver()->CreateRequest(
          kDestinationHostPortPair, kNetworkAnonymizationKey,
          net::NetLogWithSource(), params);
  net::TestCompletionCallback callback2;
  int result = request1->Start(callback2.callback());
  EXPECT_EQ(net::OK, callback2.GetResult(result));

  // Check that the hostname is not in the DNS cache for other possible NIKs.
  const url::Origin kDestinationOrigin = url::Origin::Create(kDestination);
  const net::NetworkAnonymizationKey kOtherNaks[] = {
      net::NetworkAnonymizationKey(),
      net::NetworkAnonymizationKey::CreateSameSite(
          net::SchemefulSite(kDestinationOrigin)),
  };
  for (const auto& other_nak : kOtherNaks) {
    std::unique_ptr<net::HostResolver::ResolveHostRequest> request2 =
        url_request_context->host_resolver()->CreateRequest(
            kDestinationHostPortPair, other_nak, net::NetLogWithSource(),
            params);
    net::TestCompletionCallback callback3;
    result = request2->Start(callback3.callback());
    EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, callback3.GetResult(result));
  }
}

// Checks the correct NetworkAnonymizationKey is used for host resolution in the
// case an H2 proxy is in use. In the non-H2 proxy case, the
// NetworkAnonymizationKey makes little difference, but in the H2 case, it
// affects which requests use the same session. Unlike other tests, this test
// creates a ProxyResolvingClientSocket instead of using the factory class,
// because it uses SpdySessionDependencies to create a NetworkSession configured
// to test H2.
//
// TODO(crbug.com/40946183): SPDY isn't currently supported, even through
// proxies, by ProxyResolvingClientSocket. Change that or switch to using an H1
// proxy.
TEST_P(ProxyResolvingClientSocketTest, NetworkIsolationKeyWithH2Proxy) {
  // Don't bother running this test in the SSL case - it's complicated enough
  // without it, and testing HTTPS on top of H2 provides minimal value, since
  // SSL is mocked out anyways and there are other tests that cover it on top of
  // HTTP/1.x tunnels.
  if (GetParam() == true)
    return;
  net::SpdySessionDependencies session_deps;
  session_deps.proxy_resolution_service =
      CreateProxyResolutionService("HTTPS proxy.test:80");
  std::unique_ptr<net::HttpNetworkSession> http_network_session =
      net::SpdySessionDependencies::SpdyCreateSession(&session_deps);

  net::NetworkIsolationKey kNetworkIsolationKey1 =
      net::NetworkIsolationKey::CreateTransientForTesting();
  net::NetworkIsolationKey kNetworkIsolationKey2 =
      net::NetworkIsolationKey::CreateTransientForTesting();
  net::NetworkAnonymizationKey kNetworkAnonymizationKey1 =
      net::NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
          kNetworkIsolationKey1);
  net::NetworkAnonymizationKey kNetworkAnonymizationKey2 =
      net::NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
          kNetworkIsolationKey2);
  const GURL kDestination1("https://dest1.test/");
  const GURL kDestination2("https://dest2.test/");
  const GURL kDestination3("https://dest3.test/");

  // A tunnel to kDestination1 and kDestination3 is requested using
  // kNetworkIsolationKey1, so they should use the same H2 session, and a tunnel
  // to kDestination2 is requested using kNetworkIsolationKey2, which should use
  // a different session.
  net::SpdyTestUtil spdy_util1;
  spdy::SpdySerializedFrame connect_dest1(spdy_util1.ConstructSpdyConnect(
      nullptr, 0, 1, net::HttpProxyConnectJob::kH2QuicTunnelPriority,
      net::HostPortPair::FromURL(kDestination1)));
  spdy::SpdySerializedFrame connect_dest1_resp(
      spdy_util1.ConstructSpdyGetReply(nullptr, 0, 1));
  spdy::SpdySerializedFrame connect_dest3(spdy_util1.ConstructSpdyConnect(
      nullptr, 0, 3, net::HttpProxyConnectJob::kH2QuicTunnelPriority,
      net::HostPortPair::FromURL(kDestination3)));
  spdy::SpdySerializedFrame connect_dest3_resp(
      spdy_util1.ConstructSpdyGetReply(nullptr, 0, 3));

  net::MockWrite spdy_writes[] = {
      net::CreateMockWrite(connect_dest1, 0),
      net::CreateMockWrite(connect_dest3, 2),
  };

  net::MockRead spdy_reads[] = {
      net::CreateMockRead(connect_dest1_resp, 1, net::ASYNC),
      net::CreateMockRead(connect_dest3_resp, 3, net::ASYNC),
      net::MockRead(net::SYNCHRONOUS, 0, 4),
  };

  net::SequencedSocketData socket_data(spdy_reads, spdy_writes);
  session_deps.socket_factory->AddSocketDataProvider(&socket_data);
  net::SSLSocketDataProvider ssl_data(net::ASYNC, net::OK);
  ssl_data.next_proto = net::kProtoHTTP2;
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_data);

  net::SpdyTestUtil spdy_util2;
  spdy::SpdySerializedFrame connect_dest2(spdy_util2.ConstructSpdyConnect(
      nullptr, 0, 1, net::HttpProxyConnectJob::kH2QuicTunnelPriority,
      net::HostPortPair::FromURL(kDestination2)));
  spdy::SpdySerializedFrame connect_dest2_resp(
      spdy_util2.ConstructSpdyGetReply(nullptr, 0, 1));

  net::MockWrite spdy_writes2[] = {
      net::CreateMockWrite(connect_dest2, 0),
  };

  net::MockRead spdy_reads2[] = {
      net::CreateMockRead(connect_dest2_resp, 1, net::ASYNC),
      net::MockRead(net::SYNCHRONOUS, 0, 2),
  };

  net::SequencedSocketData socket_data2(spdy_reads2, spdy_writes2);
  session_deps.socket_factory->AddSocketDataProvider(&socket_data2);
  net::SSLSocketDataProvider ssl_data2(net::ASYNC, net::OK);
  ssl_data2.next_proto = net::kProtoHTTP2;
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl_data2);

  net::ConnectJobFactory connect_job_factory;

  // Connect to kDestination1 using kNetworkIsolationKey1. It should use a new
  // H2 session.
  net::CommonConnectJobParams common_connect_job_params =
      http_network_session->CreateCommonConnectJobParams();
  ProxyResolvingClientSocket socket1(
      http_network_session.get(), &common_connect_job_params, kDestination1,
      kNetworkAnonymizationKey1, false /* use_tls */, &connect_job_factory);
  net::TestCompletionCallback callback1;
  int result = socket1.Connect(callback1.callback());
  EXPECT_THAT(callback1.GetResult(result), net::test::IsOk());

  // Connect to kDestination2 using kNetworkIsolationKey2. It should use a new
  // H2 session.
  ProxyResolvingClientSocket socket2(
      http_network_session.get(), &common_connect_job_params, kDestination2,
      kNetworkAnonymizationKey2, false /* use_tls */, &connect_job_factory);
  net::TestCompletionCallback callback2;
  result = socket2.Connect(callback2.callback());
  EXPECT_THAT(callback2.GetResult(result), net::test::IsOk());
  EXPECT_TRUE(socket_data2.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data2.AllReadDataConsumed());

  // Connect to kDestination3 using kNetworkIsolationKey1. It should reuse the
  // first H2 session.
  ProxyResolvingClientSocket socket3(
      http_network_session.get(), &common_connect_job_params, kDestination3,
      kNetworkAnonymizationKey1, false /* use_tls */, &connect_job_factory);
  net::TestCompletionCallback callback3;
  result = socket3.Connect(callback3.callback());
  EXPECT_THAT(callback3.GetResult(result), net::test::IsOk());
  EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  EXPECT_TRUE(socket_data.AllReadDataConsumed());
}

// Tests that the global socket pool limit
// (ClientSocketPoolManager::max_sockets_per_group) doesn't apply to this
// type of sockets.
TEST_P(ProxyResolvingClientSocketTest, SocketLimitNotApply) {
  const int kNumSockets = net::ClientSocketPoolManager::max_sockets_per_group(
                              net::HttpNetworkSession::NORMAL_SOCKET_POOL) +
                          10;
  const GURL kDestination("https://example.com:443");
  net::MockClientSocketFactory socket_factory;
  net::MockWrite writes[] = {
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n\r\n")};
  net::MockRead reads[] = {net::MockRead("HTTP/1.1 200 Success\r\n\r\n")};
  std::vector<std::unique_ptr<net::StaticSocketDataProvider>> socket_data;
  std::vector<std::unique_ptr<net::SSLSocketDataProvider>> ssl_data;
  for (int i = 0; i < kNumSockets; ++i) {
    socket_data.push_back(
        std::make_unique<net::StaticSocketDataProvider>(reads, writes));
    socket_data[i]->set_connect_data(net::MockConnect(net::ASYNC, net::OK));
    mock_client_socket_factory_.AddSocketDataProvider(socket_data[i].get());
    ssl_data.push_back(
        std::make_unique<net::SSLSocketDataProvider>(net::ASYNC, net::OK));
    mock_client_socket_factory_.AddSSLSocketDataProvider(ssl_data[i].get());
  }

  auto context = CreateBuilder()->Build();
  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      context.get());
  std::vector<std::unique_ptr<ProxyResolvingClientSocket>> sockets;
  for (int i = 0; i < kNumSockets; ++i) {
    std::unique_ptr<ProxyResolvingClientSocket> socket =
        proxy_resolving_socket_factory.CreateSocket(
            kDestination, net::NetworkAnonymizationKey(), use_tls_);
    net::TestCompletionCallback callback;
    int status = socket->Connect(callback.callback());
    EXPECT_THAT(callback.GetResult(status), net::test::IsOk());
    sockets.push_back(std::move(socket));
  }
  for (int i = 0; i < kNumSockets; ++i) {
    EXPECT_TRUE(socket_data[i]->AllReadDataConsumed());
    EXPECT_TRUE(socket_data[i]->AllWriteDataConsumed());
    EXPECT_EQ(use_tls_, ssl_data[i]->ConnectDataConsumed());
  }
}

TEST_P(ProxyResolvingClientSocketTest, ConnectError) {
  const struct TestData {
    // Whether the error is encountered synchronously as opposed to
    // asynchronously.
    bool is_error_sync;
    // Whether it is using a direct connection as opposed to a proxy connection.
    bool is_direct;
  } kTestCases[] = {
      {true, true}, {true, false}, {false, true}, {false, false},
  };
  const GURL kDestination("https://example.com:443");
  for (auto test : kTestCases) {
    std::string_view pac_result =
        test.is_direct ? "DIRECT" : "PROXY myproxy.com:89";
    auto context = CreateBuilder(pac_result)->Build();
    net::StaticSocketDataProvider socket_data;
    socket_data.set_connect_data(net::MockConnect(
        test.is_error_sync ? net::SYNCHRONOUS : net::ASYNC, net::ERR_FAILED));
    mock_client_socket_factory_.AddSocketDataProvider(&socket_data);

    ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
        context.get());
    std::unique_ptr<ProxyResolvingClientSocket> socket =
        proxy_resolving_socket_factory.CreateSocket(
            kDestination, net::NetworkAnonymizationKey(), use_tls_);
    net::TestCompletionCallback callback;
    int status = socket->Connect(callback.callback());
    EXPECT_EQ(net::ERR_IO_PENDING, status);
    status = callback.WaitForResult();
    if (test.is_direct) {
      EXPECT_EQ(net::ERR_FAILED, status);
    } else {
      EXPECT_EQ(net::ERR_PROXY_CONNECTION_FAILED, status);
    }
    EXPECT_TRUE(socket_data.AllReadDataConsumed());
    EXPECT_TRUE(socket_data.AllWriteDataConsumed());
  }
}

// Tests that the connection is established to the proxy. Also verifies that
// the proxy SSL connection will only negotiate H1 via ALPN, and SSL connections
// to the server receive no ALPN data.
TEST_P(ProxyResolvingClientSocketTest, ConnectToProxy) {
  const GURL kDestination("https://example.com:443");
  // Use a different port than that of |kDestination|.
  const int kProxyPort = 8009;
  const int kDirectPort = 443;
  for (bool is_direct : {true, false}) {
    net::MockClientSocketFactory socket_factory;
    std::string pac_result;
    if (is_direct) {
      pac_result = "DIRECT";
    } else {
      pac_result = base::StringPrintf("HTTPS myproxy.com:%d", kProxyPort);
    }
    auto context = CreateBuilder(pac_result)->Build();

    // Use same StaticSocketDataProvider for the direct and proxy cases. In the
    // direct case, the data won't actually be read/written.
    net::MockRead reads[] = {net::MockRead("HTTP/1.1 200 Success\r\n\r\n")};
    net::MockWrite writes[] = {
        net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                       "Host: example.com:443\r\n"
                       "Proxy-Connection: keep-alive\r\n\r\n")};
    net::IPEndPoint remote_addr(net::IPAddress(127, 0, 0, 1),
                                is_direct ? kDirectPort : kProxyPort);
    net::StaticSocketDataProvider socket_data(reads, writes);
    socket_data.set_connect_data(
        net::MockConnect(net::ASYNC, net::OK, remote_addr));
    mock_client_socket_factory_.AddSocketDataProvider(&socket_data);

    // SSL data for the proxy case.
    net::SSLSocketDataProvider proxy_ssl_data(net::ASYNC, net::OK);
    // Only H1 be allowed for the proxy.
    //
    // TODO(crbug.com/40946183): Investigate changing that.
    proxy_ssl_data.next_protos_expected_in_ssl_config =
        net::NextProtoVector{net::kProtoHTTP11};

    if (!is_direct) {
      mock_client_socket_factory_.AddSSLSocketDataProvider(&proxy_ssl_data);
    }

    // If using TLS to talk to the server, set up the SSL data for that. ALPN
    // should not be enabled for the destination server at all, as this may not
    // even be an HTTP connection.
    net::SSLSocketDataProvider server_ssl_data(net::ASYNC, net::OK);
    server_ssl_data.next_protos_expected_in_ssl_config = net::NextProtoVector();
    server_ssl_data.expected_early_data_enabled = false;
    if (use_tls_) {
      mock_client_socket_factory_.AddSSLSocketDataProvider(&server_ssl_data);
    }

    ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
        context.get());
    std::unique_ptr<ProxyResolvingClientSocket> socket =
        proxy_resolving_socket_factory.CreateSocket(
            kDestination, net::NetworkAnonymizationKey(), use_tls_);
    net::TestCompletionCallback callback;
    int status = socket->Connect(callback.callback());
    EXPECT_EQ(net::ERR_IO_PENDING, status);
    status = callback.WaitForResult();
    EXPECT_EQ(net::OK, status);
    net::IPEndPoint actual_remote_addr;
    status = socket->GetPeerAddress(&actual_remote_addr);
    if (!is_direct) {
      // ProxyResolvingClientSocket::GetPeerAddress() hides the ip of the
      // proxy, so call private member to make sure address is correct.
      EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, status);
      status = socket->socket_->GetPeerAddress(&actual_remote_addr);
    }
    EXPECT_EQ(net::OK, status);
    EXPECT_EQ(remote_addr.ToString(), actual_remote_addr.ToString());
    if (!is_direct) {
      EXPECT_TRUE(proxy_ssl_data.ConnectDataConsumed());
    }
    if (use_tls_) {
      EXPECT_TRUE(server_ssl_data.ConnectDataConsumed());
    }
  }
}

TEST_P(ProxyResolvingClientSocketTest, SocketDestroyedBeforeConnectComplete) {
  const GURL kDestination("https://example.com:443");
  net::StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(net::MockConnect(net::ASYNC, net::OK));
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);

  mock_client_socket_factory_.AddSocketDataProvider(&socket_data);
  mock_client_socket_factory_.AddSSLSocketDataProvider(&ssl_socket);

  auto context = CreateBuilder("DIRECT")->Build();

  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      context.get());
  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(
          kDestination, net::NetworkAnonymizationKey(), use_tls_);
  net::TestCompletionCallback callback;
  int status = socket->Connect(callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, status);
  socket.reset();
  // Makes sure there is no UAF and socket request is canceled properly.
  base::RunLoop().RunUntilIdle();
}

// Tests that connection itself is successful but an error occurred during
// Read()/Write().
TEST_P(ProxyResolvingClientSocketTest, ReadWriteErrors) {
  const GURL kDestination("http://example.com:80");
  const struct TestData {
    // Whether there is a read error as opposed to a write error.
    bool is_read_error;
    // Whether the error is encountered synchronously as opposed to
    // asynchronously.
    bool is_error_sync;
    // Whether it is using a direct connection as opposed to a proxy connection.
    bool is_direct;
  } kTestCases[] = {
      {true, true, true},   {true, true, false},   {false, true, true},
      {false, true, false}, {true, false, true},   {true, false, false},
      {false, false, true}, {false, false, false},
  };
  // Use a different port than that of |kDestination|.
  const int kProxyPort = 8009;
  const int kDirectPort = 80;
  for (auto test : kTestCases) {
    std::string pac_result;
    if (test.is_direct) {
      pac_result = "DIRECT";
    } else {
      pac_result = base::StringPrintf("PROXY myproxy.com:%d", kProxyPort);
    }
    auto context = CreateBuilder(pac_result)->Build();
    std::vector<net::MockWrite> writes;
    std::vector<net::MockRead> reads;
    if (!test.is_direct) {
      writes.emplace_back(
          "CONNECT example.com:80 HTTP/1.1\r\n"
          "Host: example.com:80\r\n"
          "Proxy-Connection: keep-alive\r\n\r\n");
      reads.emplace_back("HTTP/1.1 200 Success\r\n\r\n");
    }
    if (test.is_read_error) {
      reads.emplace_back(test.is_error_sync ? net::SYNCHRONOUS : net::ASYNC,
                         net::ERR_FAILED);
    } else {
      writes.emplace_back(test.is_error_sync ? net::SYNCHRONOUS : net::ASYNC,
                          net::ERR_FAILED);
    }
    net::StaticSocketDataProvider socket_data(reads, writes);
    net::IPEndPoint remote_addr(net::IPAddress(127, 0, 0, 1),
                                test.is_direct ? kDirectPort : kProxyPort);
    socket_data.set_connect_data(
        net::MockConnect(net::ASYNC, net::OK, remote_addr));
    net::MockClientSocketFactory socket_factory;
    net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
    mock_client_socket_factory_.AddSSLSocketDataProvider(&ssl_socket);
    mock_client_socket_factory_.AddSocketDataProvider(&socket_data);
    ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
        context.get());
    std::unique_ptr<ProxyResolvingClientSocket> socket =
        proxy_resolving_socket_factory.CreateSocket(
            kDestination, net::NetworkAnonymizationKey(), use_tls_);
    net::TestCompletionCallback callback;
    int status = socket->Connect(callback.callback());
    EXPECT_EQ(net::ERR_IO_PENDING, status);
    status = callback.WaitForResult();
    EXPECT_EQ(net::OK, status);
    net::IPEndPoint actual_remote_addr;
    status = socket->GetPeerAddress(&actual_remote_addr);
    if (!test.is_direct) {
      // ProxyResolvingClientSocket::GetPeerAddress() hides the ip of the
      // proxy, so call private member to make sure address is correct.
      EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, status);
      status = socket->socket_->GetPeerAddress(&actual_remote_addr);
    }
    EXPECT_EQ(net::OK, status);
    EXPECT_EQ(remote_addr.ToString(), actual_remote_addr.ToString());

    net::TestCompletionCallback read_write_callback;
    int read_write_result;
    std::string test_data_string("test data");
    auto read_buffer = base::MakeRefCounted<net::IOBufferWithSize>(10);
    auto write_buffer =
        base::MakeRefCounted<net::StringIOBuffer>(test_data_string);
    if (test.is_read_error) {
      read_write_result =
          socket->Read(read_buffer.get(), 10, read_write_callback.callback());
    } else {
      read_write_result = socket->Write(
          write_buffer.get(), test_data_string.size(),
          read_write_callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
    }
    if (read_write_result == net::ERR_IO_PENDING) {
      EXPECT_TRUE(!test.is_error_sync);
      read_write_result = read_write_callback.WaitForResult();
    }
    EXPECT_EQ(net::ERR_FAILED, read_write_result);
    EXPECT_TRUE(socket_data.AllReadDataConsumed());
    EXPECT_TRUE(socket_data.AllWriteDataConsumed());
    EXPECT_EQ(use_tls_, ssl_socket.ConnectDataConsumed());
  }
}

TEST_P(ProxyResolvingClientSocketTest, ReportsBadProxies) {
  const GURL kDestination("https://example.com:443");

  net::StaticSocketDataProvider socket_data1;
  socket_data1.set_connect_data(
      net::MockConnect(net::ASYNC, net::ERR_ADDRESS_UNREACHABLE));
  mock_client_socket_factory_.AddSocketDataProvider(&socket_data1);

  net::MockRead reads[] = {net::MockRead("HTTP/1.1 200 Success\r\n\r\n")};
  net::MockWrite writes[] = {
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n\r\n")};
  net::StaticSocketDataProvider socket_data2(reads, writes);
  socket_data2.set_connect_data(net::MockConnect(net::ASYNC, net::OK));
  mock_client_socket_factory_.AddSocketDataProvider(&socket_data2);
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  mock_client_socket_factory_.AddSSLSocketDataProvider(&ssl_socket);

  auto context = CreateBuilder()->Build();
  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      context.get());
  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(
          kDestination, net::NetworkAnonymizationKey(), use_tls_);
  net::TestCompletionCallback callback;
  int status = socket->Connect(callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, status);
  status = callback.WaitForResult();
  EXPECT_EQ(net::OK, status);

  const net::ProxyRetryInfoMap& retry_info =
      context->proxy_resolution_service()->proxy_retry_info();

  EXPECT_EQ(1u, retry_info.size());
  net::ProxyRetryInfoMap::const_iterator iter = retry_info.find(
      ProxyUriToProxyChain("bad:99", net::ProxyServer::SCHEME_HTTP));
  EXPECT_TRUE(iter != retry_info.end());
  EXPECT_EQ(use_tls_, ssl_socket.ConnectDataConsumed());
}

TEST_P(ProxyResolvingClientSocketTest, ResetSocketAfterTunnelAuth) {
  const GURL kDestination("https://example.com:443");

  // Initial connect without credentials. The server responds with a 407.
  net::MockWrite kConnectWrites1[] = {
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n"
                     "\r\n")};
  net::MockRead kConnectReads1[] = {
      net::MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"
                    "Proxy-Authenticate: Basic realm=\"test_realm\"\r\n"
                    "\r\n")};

  net::StaticSocketDataProvider kSocketData1(kConnectReads1, kConnectWrites1);
  mock_client_socket_factory_.AddSocketDataProvider(&kSocketData1);

  auto context = CreateBuilder()->Build();
  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      context.get());
  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(
          kDestination, net::NetworkAnonymizationKey(), use_tls_);
  net::TestCompletionCallback callback;
  int status = socket->Connect(callback.callback());
  EXPECT_THAT(callback.GetResult(status),
              net::test::IsError(net::ERR_PROXY_AUTH_REQUESTED));
  // Make sure |socket_| is closed appropriately.
  EXPECT_FALSE(socket->socket_);
}

TEST_P(ProxyResolvingClientSocketTest, MultiroundAuth) {
  const GURL kDestination("https://example.com:443");

  // Initial connect without credentials. The server responds with a 407.
  net::MockWrite kConnectWrites1[] = {
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n"
                     "\r\n")};
  net::MockRead kConnectReads1[] = {
      net::MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"
                    "Proxy-Authenticate: Basic realm=\"test_realm\"\r\n"
                    "\r\n")};

  // Second connect attempt includes credentials for test_realm.
  net::MockWrite kConnectWrites2[] = {
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n"
                     "Proxy-Authorization: Basic dXNlcjpwYXNzd29yZA==\r\n"
                     "\r\n")};
  net::MockRead kConnectReads2[] = {
      net::MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"
                    "Proxy-Authenticate: Basic realm=\"test_realm2\"\r\n"
                    "\r\n")};

  // Third connect attempt include credentials for test_realm2.
  net::MockWrite kConnectWrites3[] = {
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n"
                     "Proxy-Authorization: Basic dXNlcjI6cGFzc3dvcmQy\r\n"
                     "\r\n")};
  net::MockRead kConnectReads3[] = {
      net::MockRead("HTTP/1.1 200 Success\r\n\r\n")};

  net::StaticSocketDataProvider kSocketData1(kConnectReads1, kConnectWrites1);
  mock_client_socket_factory_.AddSocketDataProvider(&kSocketData1);

  net::StaticSocketDataProvider kSocketData2(kConnectReads2, kConnectWrites2);
  mock_client_socket_factory_.AddSocketDataProvider(&kSocketData2);
  net::StaticSocketDataProvider kSocketData3(kConnectReads3, kConnectWrites3);
  mock_client_socket_factory_.AddSocketDataProvider(&kSocketData3);

  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  mock_client_socket_factory_.AddSSLSocketDataProvider(&ssl_socket);

  auto context = CreateBuilder()->Build();

  net::HttpAuthCache* auth_cache =
      context->http_transaction_factory()->GetSession()->http_auth_cache();

  auth_cache->Add(url::SchemeHostPort(GURL("http://bad:99")),
                  net::HttpAuth::AUTH_PROXY, "test_realm",
                  net::HttpAuth::AUTH_SCHEME_BASIC,
                  net::NetworkAnonymizationKey(), "Basic realm=\"test_realm\"",
                  net::AuthCredentials(u"user", u"password"), std::string());

  auth_cache->Add(url::SchemeHostPort(GURL("http://bad:99")),
                  net::HttpAuth::AUTH_PROXY, "test_realm2",
                  net::HttpAuth::AUTH_SCHEME_BASIC,
                  net::NetworkAnonymizationKey(), "Basic realm=\"test_realm2\"",
                  net::AuthCredentials(u"user2", u"password2"), std::string());

  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      context.get());
  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(
          kDestination, net::NetworkAnonymizationKey(), use_tls_);
  net::TestCompletionCallback callback;
  int status = socket->Connect(callback.callback());
  EXPECT_THAT(callback.GetResult(status), net::test::IsOk());
  EXPECT_EQ(use_tls_, ssl_socket.ConnectDataConsumed());
}

TEST_P(ProxyResolvingClientSocketTest, ReusesHTTPAuthCache_Lookup) {
  const GURL kDestination("https://example.com:443");

  // Initial connect without credentials. The server responds with a 407.
  net::MockWrite kConnectWrites1[] = {
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n"
                     "\r\n")};
  net::MockRead kConnectReads1[] = {
      net::MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"
                    "Proxy-Authenticate: Basic realm=\"test_realm\"\r\n"
                    "\r\n")};

  // Second connect attempt includes credentials.
  net::MockWrite kConnectWrites2[] = {
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n"
                     "Proxy-Authorization: Basic dXNlcjpwYXNzd29yZA==\r\n"
                     "\r\n")};
  net::MockRead kConnectReads2[] = {
      net::MockRead("HTTP/1.1 200 Success\r\n\r\n")};

  net::StaticSocketDataProvider kSocketData1(kConnectReads1, kConnectWrites1);
  mock_client_socket_factory_.AddSocketDataProvider(&kSocketData1);

  net::StaticSocketDataProvider kSocketData2(kConnectReads2, kConnectWrites2);
  mock_client_socket_factory_.AddSocketDataProvider(&kSocketData2);
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  mock_client_socket_factory_.AddSSLSocketDataProvider(&ssl_socket);

  auto context = CreateBuilder()->Build();

  net::HttpAuthCache* auth_cache =
      context->http_transaction_factory()->GetSession()->http_auth_cache();

  // We are adding these credentials at an empty path so that it won't be picked
  // up by the preemptive authentication step and will only be picked up via
  // origin + realm + scheme lookup.
  auth_cache->Add(url::SchemeHostPort(GURL("http://bad:99")),
                  net::HttpAuth::AUTH_PROXY, "test_realm",
                  net::HttpAuth::AUTH_SCHEME_BASIC,
                  net::NetworkAnonymizationKey(), "Basic realm=\"test_realm\"",
                  net::AuthCredentials(u"user", u"password"), std::string());

  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      context.get());
  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(
          kDestination, net::NetworkAnonymizationKey(), use_tls_);
  net::TestCompletionCallback callback;
  int status = socket->Connect(callback.callback());
  EXPECT_THAT(callback.GetResult(status), net::test::IsOk());
  EXPECT_EQ(use_tls_, ssl_socket.ConnectDataConsumed());
}

// Make sure that if HttpAuthCache is updated e.g through normal URLRequests,
// ProxyResolvingClientSocketFactory uses the latest cache for creating new
// sockets.
TEST_P(ProxyResolvingClientSocketTest, FactoryUsesLatestHTTPAuthCache) {
  auto context = CreateBuilder()->Build();

  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      context.get());

  // After creating |socket_factory|, updates the auth cache with credentials.
  // New socket connections should pick up this change.
  net::HttpAuthCache* auth_cache =
      context->http_transaction_factory()->GetSession()->http_auth_cache();

  // We are adding these credentials at an empty path so that it won't be picked
  // up by the preemptive authentication step and will only be picked up via
  // origin + realm + scheme lookup.
  auth_cache->Add(url::SchemeHostPort(GURL("http://bad:99")),
                  net::HttpAuth::AUTH_PROXY, "test_realm",
                  net::HttpAuth::AUTH_SCHEME_BASIC,
                  net::NetworkAnonymizationKey(), "Basic realm=\"test_realm\"",
                  net::AuthCredentials(u"user", u"password"), std::string());

  const GURL kDestination("https://example.com:443");

  // Initial connect without credentials. The server responds with a 407.
  net::MockWrite kConnectWrites[] = {
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n"
                     "\r\n"),
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n"
                     "Proxy-Authorization: Basic dXNlcjpwYXNzd29yZA==\r\n"
                     "\r\n")};
  net::MockRead kConnectReads[] = {
      net::MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"
                    "Proxy-Authenticate: Basic realm=\"test_realm\"\r\n"
                    "Proxy-Connection: keep-alive\r\n"
                    "Content-Length: 0\r\n"
                    "\r\n"),
      net::MockRead("HTTP/1.1 200 Success\r\n\r\n")};

  net::StaticSocketDataProvider kSocketData(kConnectReads, kConnectWrites);
  mock_client_socket_factory_.AddSocketDataProvider(&kSocketData);
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  mock_client_socket_factory_.AddSSLSocketDataProvider(&ssl_socket);

  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(
          kDestination, net::NetworkAnonymizationKey(), use_tls_);
  net::TestCompletionCallback callback;
  int status = socket->Connect(callback.callback());
  EXPECT_THAT(callback.GetResult(status), net::test::IsOk());
  EXPECT_EQ(use_tls_, ssl_socket.ConnectDataConsumed());
}

TEST_P(ProxyResolvingClientSocketTest, ReusesHTTPAuthCache_Preemptive) {
  const GURL kDestination("https://example.com:443");

  // Initial connect uses preemptive credentials. That is all.
  net::MockWrite kConnectWrites[] = {
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n"
                     "Proxy-Authorization: Basic dXNlcjpwYXNzd29yZA==\r\n"
                     "\r\n")};
  net::MockRead kConnectReads[] = {
      net::MockRead("HTTP/1.1 200 Success\r\n\r\n")};

  net::StaticSocketDataProvider kSocketData(kConnectReads, kConnectWrites);
  mock_client_socket_factory_.AddSocketDataProvider(&kSocketData);
  net::SSLSocketDataProvider ssl_socket(net::ASYNC, net::OK);
  mock_client_socket_factory_.AddSSLSocketDataProvider(&ssl_socket);

  auto context = CreateBuilder()->Build();

  net::HttpAuthCache* auth_cache =
      context->http_transaction_factory()->GetSession()->http_auth_cache();

  auth_cache->Add(url::SchemeHostPort(GURL("http://bad:99")),
                  net::HttpAuth::AUTH_PROXY, "test_realm",
                  net::HttpAuth::AUTH_SCHEME_BASIC,
                  net::NetworkAnonymizationKey(), "Basic realm=\"test_realm\"",
                  net::AuthCredentials(u"user", u"password"), "/");

  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      context.get());
  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(
          kDestination, net::NetworkAnonymizationKey(), use_tls_);

  net::TestCompletionCallback callback;
  int status = socket->Connect(callback.callback());
  EXPECT_THAT(callback.GetResult(status), net::test::IsOk());
  EXPECT_EQ(use_tls_, ssl_socket.ConnectDataConsumed());
}

TEST_P(ProxyResolvingClientSocketTest, ReusesHTTPAuthCache_NoCredentials) {
  const GURL kDestination("https://example.com:443");

  // Initial connect uses preemptive credentials. That is all.
  net::MockWrite kConnectWrites[] = {
      net::MockWrite("CONNECT example.com:443 HTTP/1.1\r\n"
                     "Host: example.com:443\r\n"
                     "Proxy-Connection: keep-alive\r\n"
                     "\r\n")};
  net::MockRead kConnectReads[] = {
      net::MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"
                    "Proxy-Authenticate: Basic realm=\"test_realm\"\r\n"
                    "\r\n")};

  net::StaticSocketDataProvider kSocketData(kConnectReads, kConnectWrites);
  mock_client_socket_factory_.AddSocketDataProvider(&kSocketData);

  auto context = CreateBuilder()->Build();

  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      context.get());
  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(
          kDestination, net::NetworkAnonymizationKey(), use_tls_);

  net::TestCompletionCallback callback;
  int status = socket->Connect(callback.callback());
  EXPECT_THAT(callback.GetResult(status), net::ERR_PROXY_AUTH_REQUESTED);
}

// Make sure that url is sanitized before it is disclosed to the proxy.
TEST_P(ProxyResolvingClientSocketTest, URLSanitized) {
  GURL url("http://username:password@www.example.com:79/?ref#hash#hash");

  auto context_builder = CreateBuilder();
  net::ProxyConfig proxy_config;
  proxy_config.set_pac_url(GURL("http://foopy/proxy.pac"));
  proxy_config.set_pac_mandatory(true);
  net::MockAsyncProxyResolver resolver;
  auto proxy_resolver_factory =
      std::make_unique<net::MockAsyncProxyResolverFactory>(false);
  net::MockAsyncProxyResolverFactory* proxy_resolver_factory_raw =
      proxy_resolver_factory.get();
  context_builder->set_proxy_resolution_service(
      std::make_unique<net::ConfiguredProxyResolutionService>(
          std::make_unique<net::ProxyConfigServiceFixed>(
              net::ProxyConfigWithAnnotation(proxy_config,
                                             TRAFFIC_ANNOTATION_FOR_TESTS)),
          std::move(proxy_resolver_factory), nullptr,
          /*quick_check_enabled=*/true));
  auto context = context_builder->Build();

  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      context.get());
  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(
          url, net::NetworkAnonymizationKey(), use_tls_);
  net::TestCompletionCallback callback;
  int status = socket->Connect(callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, status);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, proxy_resolver_factory_raw->pending_requests().size());
  EXPECT_EQ(
      GURL("http://foopy/proxy.pac"),
      proxy_resolver_factory_raw->pending_requests()[0]->script_data()->url());
  proxy_resolver_factory_raw->pending_requests()[0]->CompleteNowWithForwarder(
      net::OK, &resolver);
  ASSERT_EQ(1u, resolver.pending_jobs().size());
  // The URL should have been simplified, stripping the username/password/hash.
  EXPECT_EQ(GURL("http://www.example.com:79/?ref"),
            resolver.pending_jobs()[0]->url());
}

// Tests that socket is destroyed before proxy resolution can complete
// asynchronously.
TEST_P(ProxyResolvingClientSocketTest,
       SocketDestroyedBeforeProxyResolutionCompletes) {
  GURL url("http://www.example.com:79");

  auto context_builder = CreateBuilder();
  net::ProxyConfig proxy_config;
  proxy_config.set_pac_url(GURL("http://foopy/proxy.pac"));
  proxy_config.set_pac_mandatory(true);
  net::MockAsyncProxyResolver resolver;
  auto proxy_resolver_factory =
      std::make_unique<net::MockAsyncProxyResolverFactory>(false);
  net::MockAsyncProxyResolverFactory* proxy_resolver_factory_raw =
      proxy_resolver_factory.get();
  context_builder->set_proxy_resolution_service(
      std::make_unique<net::ConfiguredProxyResolutionService>(
          std::make_unique<net::ProxyConfigServiceFixed>(
              net::ProxyConfigWithAnnotation(proxy_config,
                                             TRAFFIC_ANNOTATION_FOR_TESTS)),
          std::move(proxy_resolver_factory), nullptr,
          /*quick_check_enabled=*/true));
  auto context = context_builder->Build();

  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      context.get());
  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(
          url, net::NetworkAnonymizationKey(), use_tls_);
  net::TestCompletionCallback callback;
  EXPECT_EQ(net::ERR_IO_PENDING, socket->Connect(callback.callback()));
  socket.reset();
  ASSERT_EQ(1u, proxy_resolver_factory_raw->pending_requests().size());
  proxy_resolver_factory_raw->pending_requests()[0]->CompleteNowWithForwarder(
      net::OK, &resolver);
  base::RunLoop().RunUntilIdle();
}

// Regression test for crbug.com/849300. If proxy resolution is successful but
// the proxy scheme is not supported, do not continue with connection
// establishment.
TEST_P(ProxyResolvingClientSocketTest, NoSupportedProxies) {
  const GURL kDestination("https://example.com:443");

  auto context_builder = CreateBuilder();
  net::ProxyConfig proxy_config;
  // Use an unsupported proxy scheme.
  proxy_config.proxy_rules().ParseFromString("quic://foopy:8080");
  auto proxy_resolver_factory =
      std::make_unique<net::MockAsyncProxyResolverFactory>(false);
  context_builder->set_proxy_resolution_service(
      std::make_unique<net::ConfiguredProxyResolutionService>(
          std::make_unique<net::ProxyConfigServiceFixed>(
              net::ProxyConfigWithAnnotation(proxy_config,
                                             TRAFFIC_ANNOTATION_FOR_TESTS)),
          std::move(proxy_resolver_factory), nullptr,
          /*quick_check_enabled=*/true));
  auto context = context_builder->Build();

  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      context.get());
  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(
          kDestination, net::NetworkAnonymizationKey(), use_tls_);
  net::TestCompletionCallback callback;
  int status = socket->Connect(callback.callback());
  status = callback.GetResult(status);
  EXPECT_EQ(net::ERR_NO_SUPPORTED_PROXIES, status);
}

class ReconsiderProxyAfterErrorTest
    : public testing::Test,
      public testing::WithParamInterface<::testing::tuple<bool, bool, int>> {
 public:
  ReconsiderProxyAfterErrorTest() : use_tls_(::testing::get<0>(GetParam())) {}
  ~ReconsiderProxyAfterErrorTest() override = default;

  base::test::TaskEnvironment task_environment_;
  net::MockClientSocketFactory mock_client_socket_factory_;
  const bool use_tls_;
};

// List of errors that are used in the proxy resolution tests.
//
// Note: ProxyResolvingClientSocket currently removes
// net::ProxyServer::SCHEME_QUIC, so this list excludes errors that are
// retryable only for QUIC proxies.
const int kProxyTestMockErrors[] = {
    net::ERR_PROXY_CONNECTION_FAILED, net::ERR_NAME_NOT_RESOLVED,
    net::ERR_ADDRESS_UNREACHABLE,     net::ERR_CONNECTION_RESET,
    net::ERR_CONNECTION_REFUSED,      net::ERR_CONNECTION_ABORTED,
    net::ERR_SOCKS_CONNECTION_FAILED, net::ERR_TIMED_OUT,
    net::ERR_CONNECTION_TIMED_OUT,    net::ERR_PROXY_CERTIFICATE_INVALID,
    net::ERR_SSL_PROTOCOL_ERROR};

INSTANTIATE_TEST_SUITE_P(
    All,
    ReconsiderProxyAfterErrorTest,
    testing::Combine(testing::Bool(),
                     testing::Bool(),
                     testing::ValuesIn(kProxyTestMockErrors)));

TEST_P(ReconsiderProxyAfterErrorTest, ReconsiderProxyAfterError) {
  net::IoMode io_mode =
      ::testing::get<1>(GetParam()) ? net::SYNCHRONOUS : net::ASYNC;
  const int mock_error = ::testing::get<2>(GetParam());
  auto context_builder = net::CreateTestURLRequestContextBuilder();
  context_builder->set_proxy_resolution_service(CreateProxyResolutionService(
      "HTTPS badproxy:99; HTTPS badfallbackproxy:98; DIRECT"));
  context_builder->set_client_socket_factory_for_testing(
      &mock_client_socket_factory_);
  auto context = context_builder->Build();

  // Before starting the test, verify that there are no proxies marked as bad.
  ASSERT_TRUE(context->proxy_resolution_service()->proxy_retry_info().empty())
      << mock_error;

  // Configure the HTTP CONNECT to fail with `mock_error`.
  //
  // TODO(crbug.com/40810987): Test this more accurately. Errors like
  // `ERR_PROXY_CONNECTION_FAILED` or `ERR_PROXY_CERTIFICATE_INVALID` are
  // surfaced in response to other errors in TCP or TLS connection setup.
  static const char kHttpConnect[] =
      "CONNECT example.com:443 HTTP/1.1\r\n"
      "Host: example.com:443\r\n"
      "Proxy-Connection: keep-alive\r\n\r\n";
  const net::MockWrite kWrites[] = {{net::ASYNC, kHttpConnect}};
  const net::MockRead kReads[] = {{net::ASYNC, mock_error}};

  // Connect to first broken proxy.
  net::StaticSocketDataProvider data1(kReads, kWrites);
  net::SSLSocketDataProvider ssl_data1(io_mode, net::OK);
  data1.set_connect_data(net::MockConnect(io_mode, net::OK));
  mock_client_socket_factory_.AddSocketDataProvider(&data1);
  mock_client_socket_factory_.AddSSLSocketDataProvider(&ssl_data1);

  // Connect to second broken proxy.
  net::StaticSocketDataProvider data2(kReads, kWrites);
  net::SSLSocketDataProvider ssl_data2(io_mode, net::OK);
  data2.set_connect_data(net::MockConnect(io_mode, net::OK));
  mock_client_socket_factory_.AddSocketDataProvider(&data2);
  mock_client_socket_factory_.AddSSLSocketDataProvider(&ssl_data2);

  // Connect using direct.
  net::StaticSocketDataProvider data3;
  net::SSLSocketDataProvider ssl_data3(io_mode, net::OK);
  data3.set_connect_data(net::MockConnect(io_mode, net::OK));
  mock_client_socket_factory_.AddSocketDataProvider(&data3);
  mock_client_socket_factory_.AddSSLSocketDataProvider(&ssl_data3);

  const GURL kDestination("https://example.com:443");
  ProxyResolvingClientSocketFactory proxy_resolving_socket_factory(
      context.get());
  std::unique_ptr<ProxyResolvingClientSocket> socket =
      proxy_resolving_socket_factory.CreateSocket(
          kDestination, net::NetworkAnonymizationKey(), use_tls_);
  net::TestCompletionCallback callback;
  int status = socket->Connect(callback.callback());
  EXPECT_EQ(net::ERR_IO_PENDING, status);
  status = callback.WaitForResult();
  EXPECT_EQ(net::OK, status);

  const net::ProxyRetryInfoMap& retry_info =
      context->proxy_resolution_service()->proxy_retry_info();
  EXPECT_EQ(2u, retry_info.size()) << mock_error;
  EXPECT_NE(retry_info.end(),
            retry_info.find(ProxyUriToProxyChain(
                "https://badproxy:99", net::ProxyServer::SCHEME_HTTPS)));
  EXPECT_NE(retry_info.end(), retry_info.find(ProxyUriToProxyChain(
                                  "https://badfallbackproxy:98",
                                  net::ProxyServer::SCHEME_HTTPS)));
  // Should always use HTTPS to talk to HTTPS proxy.
  EXPECT_TRUE(ssl_data1.ConnectDataConsumed());
  EXPECT_TRUE(ssl_data2.ConnectDataConsumed());
  // This depends on whether the consumer has requested to use TLS.
  EXPECT_EQ(use_tls_, ssl_data3.ConnectDataConsumed());
}

}  // namespace network
