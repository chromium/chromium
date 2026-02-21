// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_connect_job.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_stream_pool_test_util.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log.h"
#include "net/socket/connect_job_test_util.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/transport_client_socket_pool_test_util.h"
#include "net/ssl/ssl_config_service.h"
#include "net/ssl/test_ssl_config_service.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net {
namespace {

const char kHostName[] = "host.test";
using test::IsError;
using test::IsOk;

class TcpConnectJobTest : public TestWithTaskEnvironment {
 public:
  TcpConnectJobTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        common_connect_job_params_(
            &client_socket_factory_,
            &host_resolver_,
            /*http_auth_cache=*/nullptr,
            /*http_auth_handler_factory=*/nullptr,
            /*spdy_session_pool=*/nullptr,
            /*quic_supported_versions=*/nullptr,
            /*quic_session_pool=*/nullptr,
            /*proxy_delegate=*/nullptr,
            /*http_user_agent_settings=*/nullptr,
            &ssl_client_context_,
            /*socket_performance_watcher_factory=*/nullptr,
            /*network_quality_estimator=*/nullptr,
            NetLog::Get(),
            /*websocket_endpoint_lock_manager=*/nullptr,
            /*http_server_properties=*/nullptr,
            /*alpn_protos=*/nullptr,
            /*application_settings=*/nullptr,
            /*ignore_certificate_errors=*/nullptr,
            /*enable_early_data=*/nullptr) {}

  ~TcpConnectJobTest() override {
    EXPECT_TRUE(client_socket_factory_.AllDataProvidersUsed());
  }

  static IPEndPoint MakeIPEndPoint(std::string_view addr, int port) {
    return IPEndPoint(*IPAddress::FromIPLiteral(addr), port);
  }

  // Helper method to wrap ServiceEndpointBuilder() with the most commonly
  // needed parameters.
  static ServiceEndpoint CreateServiceEndpoint(
      const std::vector<IPEndPoint>& ip_endpoints,
      std::vector<std::string> alpns = {},
      bool ech = false) {
    ServiceEndpointBuilder builder;
    for (const auto& ip_endpoint : ip_endpoints) {
      builder.add_ip_endpoint(ip_endpoint);
    }
    if (!alpns.empty()) {
      builder.set_alpns(std::move(alpns));
    }
    if (ech) {
      // The actual value here doesn't matter.
      builder.set_ech_config_list({'?'});
    }
    return builder.endpoint();
  }

  // Returns supported ALPNs, taking `destination_` and `supported_alpns_` into
  // consideration. Returns `supported_alpns_` is set. Otherwise, returns h2 and
  // h1 for HTTPS scheme, the empty set for everything else.
  base::flat_set<std::string> GetSupportedAplns() const {
    if (supported_alpns_) {
      return *supported_alpns_;
    }
    const url::SchemeHostPort* scheme_host_port =
        std::get_if<url::SchemeHostPort>(&destination_);
    if (scheme_host_port && scheme_host_port->scheme() == url::kHttpsScheme) {
      return {"h2", "http/1.1"};
    }
    return {};
  }

  scoped_refptr<TransportSocketParams> SocketParams() {
    return base::MakeRefCounted<TransportSocketParams>(
        destination_, NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
        OnHostResolutionCallback(), GetSupportedAplns());
  }

  // Initializes `test_delegate_` and `connect_job_`, tearing down old ones
  // first.
  void InitConnectJob() {
    // Destruction order is important here, as the ConnectJob references the
    // delegate.
    test_delegate_.reset();
    connect_job_.reset();

    test_delegate_ = std::make_unique<TestConnectJobDelegate>();
    connect_job_ = std::make_unique<TcpConnectJob>(
        DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
        SocketParams(), test_delegate_.get(), /*net_log=*/nullptr,
        service_endpoint_override_);
  }

  // Combines InitConnectJob() and test_delegate_->StartJobExpectingResult(),
  // expecting an error.
  void InitRunAndExpectError(Error expected_result, bool expect_sync_result) {
    DCHECK_NE(expected_result, OK);
    InitConnectJob();
    test_delegate_->StartJobExpectingResult(connect_job_.get(), expected_result,
                                            expect_sync_result);
  }

  // Combines InitConnectJob() and test_delegate_->StartJobExpectingResult(),
  // expecting success.
  void InitRunAndExpectSuccess(const IPEndPoint& expected_ip_endpoint,
                               const ServiceEndpoint& expected_service_endpoint,
                               bool expect_sync_result) {
    InitConnectJob();
    test_delegate_->StartJobExpectingResult(connect_job_.get(), OK,
                                            expect_sync_result);
    CheckConnection(expected_ip_endpoint, expected_service_endpoint);
  }

  // Checks that there's a socket, and it's connected to the specified
  // endpoints.
  void CheckConnection(const IPEndPoint& expected_ip_endpoint,
                       const ServiceEndpoint& expected_service_endpoint) const {
    EXPECT_EQ(connect_job_->GetServiceEndpoint(), expected_service_endpoint);
    ASSERT_TRUE(test_delegate_->socket());
    IPEndPoint actual_ip_endpoint;
    ASSERT_THAT(test_delegate_->socket()->GetPeerAddress(&actual_ip_endpoint),
                IsOk());
    EXPECT_EQ(actual_ip_endpoint, expected_ip_endpoint);
    // If there's a connection, HasEstablishedConnection() must be true. May or
    // may not be true when there's no connection, so can't do anything by
    // default in the failure path.
    EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  }

  // Combines InitConnectJob() and connect_job_->Connect(). Mostly used for
  // async tests where some events have to be micromanaged.
  int InitAndStart() {
    InitConnectJob();
    return connect_job_->Connect();
  }

  // Adds a connection attempt to `client_socket_factory_`.
  void AddConnect(MockConnect mock_connect,
                  const IPEndPoint& expected_destination) {
    auto data = std::make_unique<SequencedSocketData>();
    // This is the "actual" address the socket will pretend it was connected to,
    // on success.
    mock_connect.peer_addr = expected_destination;
    data->set_connect_data(mock_connect);
    // This is the list of addresses that the connection attempt is expected to
    // be provided.
    data->set_expected_addresses(AddressList(expected_destination));
    client_socket_factory_.AddSocketDataProvider(data.get());
    socket_data_.emplace_back(std::move(data));
  }

 protected:
  // IPs use by tests. Numbers are chose to make it easy to identify them from
  // failure output.
  const IPEndPoint kIpV4Endpoint1 = MakeIPEndPoint("4.1.1.1", 41);
  const IPEndPoint kIpV4Endpoint2 = MakeIPEndPoint("4.2.2.2", 42);
  const IPEndPoint kIpV4Endpoint3 = MakeIPEndPoint("4.3.3.3", 43);
  const IPEndPoint kIpV4Endpoint4 = MakeIPEndPoint("4.4.4.4", 44);
  const IPEndPoint kIpV6Endpoint1 = MakeIPEndPoint("6::1", 61);
  const IPEndPoint kIpV6Endpoint2 = MakeIPEndPoint("6::2", 62);
  const IPEndPoint kIpV6Endpoint3 = MakeIPEndPoint("6::3", 63);
  const IPEndPoint kIpV6Endpoint4 = MakeIPEndPoint("6::4", 64);

  // Common extended DNS error information for DNS error tests.
  // `is_secure_network_error` is set to true because the default is false.
  const ResolveErrorInfo kResolveErrorInfo{ERR_DNS_TIMED_OUT,
                                           /*is_secure_network_error=*/true};

  const std::set<std::string> kDnsAliases{"bar", "foo"};

  // Socket data for `client_socket_factory_`. Only the connect data matters.
  std::vector<std::unique_ptr<SequencedSocketData>> socket_data_;

  // The destination to connect to. Used by SocketParams(). Importing thing to
  // note is that this is HTTPS by default.
  TransportSocketParams::Endpoint destination_{
      url::SchemeHostPort(url::kHttpsScheme, kHostName, 443)};
  // If non-null, overrides the default per-destination-scheme supported ALPN
  // list.
  std::optional<base::flat_set<std::string>> supported_alpns_;

  FakeServiceEndpointResolver host_resolver_;
  MockClientSocketFactory client_socket_factory_;
  TestSSLConfigService ssl_config_service_{SSLContextConfig()};
  MockCertVerifier cert_verifier_;
  TransportSecurityState transport_security_state_;
  SSLClientContext ssl_client_context_{&ssl_config_service_, &cert_verifier_,
                                       &transport_security_state_,
                                       /*ssl_client_session_cache=*/nullptr,
                                       /*sct_auditing_delegate=*/nullptr};
  const CommonConnectJobParams common_connect_job_params_;
  // Passed in to TcpConnectJob constructor.
  std::optional<TcpConnectJob::ServiceEndpointOverride>
      service_endpoint_override_;

  // Use pointers so can easily re-initialize these.
  std::unique_ptr<TestConnectJobDelegate> test_delegate_;
  std::unique_ptr<TcpConnectJob> connect_job_;
};

TEST_F(TcpConnectJobTest, DnsErrorSync) {
  // Use something other than ERR_NAME_NOT_RESOLVED because that's the default
  // error in some cases.
  host_resolver_.ConfigureDefaultResolution()
      .CompleteStartSynchronously(ERR_FAILED)
      .set_resolve_error_info(kResolveErrorInfo);
  InitRunAndExpectError(ERR_FAILED, /*expect_sync_result=*/true);
  EXPECT_EQ(connect_job_->GetResolveErrorInfo(), kResolveErrorInfo);
}

TEST_F(TcpConnectJobTest, DnsErrorAsync) {
  // Use something other than ERR_NAME_NOT_RESOLVED because that's the default
  // error in some cases.
  host_resolver_.AddFakeRequest()
      ->set_resolve_error_info(kResolveErrorInfo)
      .CompleteStartAsynchronously(ERR_FAILED);
  InitRunAndExpectError(ERR_FAILED, /*expect_sync_result=*/false);
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
  EXPECT_EQ(connect_job_->GetResolveErrorInfo(), kResolveErrorInfo);
}

TEST_F(TcpConnectJobTest, DnsErrorDuringConnect) {
  for (auto crypto_ready : {false, true}) {
    SCOPED_TRACE(crypto_ready);
    MockConnectCompleter connect_completer;
    AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);
    auto request = host_resolver_.AddFakeRequest();
    request->set_start_callback(base::BindLambdaForTesting([&]() {
      request->set_crypto_ready(crypto_ready)
          .add_endpoint(CreateServiceEndpoint({kIpV4Endpoint1}))
          .CallOnServiceEndpointsUpdated();
      connect_completer.WaitForConnect();
      request->set_resolve_error_info(kResolveErrorInfo)
          .CallOnServiceEndpointRequestFinished(ERR_FAILED);
    }));
    InitRunAndExpectError(ERR_FAILED, /*expect_sync_result=*/false);
    EXPECT_FALSE(connect_job_->HasEstablishedConnection());
    EXPECT_EQ(connect_job_->GetResolveErrorInfo(), kResolveErrorInfo);
  }
}

// Test the case where there's a DNS error after the connection attempt starts.
TEST_F(TcpConnectJobTest, DnsErrorAfterConnectStart) {
  for (auto crypto_ready : {false, true}) {
    SCOPED_TRACE(crypto_ready);
    MockConnectCompleter connect_completer;
    AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);
    auto request = host_resolver_.AddFakeRequest();
    request->set_start_callback(base::BindLambdaForTesting([&]() {
      request->set_crypto_ready(crypto_ready)
          .add_endpoint(CreateServiceEndpoint({kIpV4Endpoint1}))
          .CallOnServiceEndpointsUpdated();

      connect_completer.WaitForConnect();
      EXPECT_FALSE(connect_job_->HasEstablishedConnection());

      request->set_resolve_error_info(kResolveErrorInfo)
          .CallOnServiceEndpointRequestFinished(ERR_FAILED);
      EXPECT_FALSE(connect_job_->HasEstablishedConnection());
    }));
    InitRunAndExpectError(ERR_FAILED, /*expect_sync_result=*/false);
    EXPECT_FALSE(connect_job_->HasEstablishedConnection());
    EXPECT_EQ(connect_job_->GetResolveErrorInfo(), kResolveErrorInfo);
  }
}

// Test the case where there's a DNS error after a connection is established.
// This fails the request if crypto ready has not been received yet.
TEST_F(TcpConnectJobTest, DnsErrorAfterConnectComplete) {
  MockConnectCompleter connect_completer;
  AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);
  auto request = host_resolver_.AddFakeRequest();
  request->set_start_callback(base::BindLambdaForTesting([&]() {
    request->set_crypto_ready(false)
        .add_endpoint(CreateServiceEndpoint({kIpV4Endpoint1}))
        .CallOnServiceEndpointsUpdated();
    EXPECT_FALSE(connect_job_->HasEstablishedConnection());
    connect_completer.WaitForConnectAndComplete(OK);
    EXPECT_TRUE(connect_job_->HasEstablishedConnection());
    request->set_resolve_error_info(kResolveErrorInfo)
        .CallOnServiceEndpointRequestFinished(ERR_FAILED);
  }));
  InitRunAndExpectError(ERR_FAILED, /*expect_sync_result=*/false);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  EXPECT_EQ(connect_job_->GetResolveErrorInfo(), kResolveErrorInfo);
}

TEST_F(TcpConnectJobTest, ConnectionSuccessSyncDnsSyncConnect) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  host_resolver_.ConfigureDefaultResolution()
      .add_endpoint(service_endpoint)
      .set_aliases(kDnsAliases)
      .CompleteStartSynchronously(OK);
  AddConnect(MockConnect(SYNCHRONOUS, OK), kIpV4Endpoint1);
  InitRunAndExpectSuccess(kIpV4Endpoint1, service_endpoint,
                          /*expect_sync_result=*/true);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  EXPECT_EQ(kDnsAliases, test_delegate_->socket()->GetDnsAliases());
}

TEST_F(TcpConnectJobTest, ConnectionErrorSyncDnsSyncConnect) {
  host_resolver_.ConfigureDefaultResolution()
      .add_endpoint(CreateServiceEndpoint({kIpV4Endpoint1}))
      .CompleteStartSynchronously(OK);
  AddConnect(MockConnect(SYNCHRONOUS, ERR_FAILED), kIpV4Endpoint1);
  InitRunAndExpectError(ERR_FAILED, /*expect_sync_result=*/true);
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
}

TEST_F(TcpConnectJobTest, ConnectionSuccessSyncDnsAsyncConnect) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  host_resolver_.ConfigureDefaultResolution()
      .add_endpoint(service_endpoint)
      .set_aliases(kDnsAliases)
      .CompleteStartSynchronously(OK);
  AddConnect(MockConnect(ASYNC, OK), kIpV4Endpoint1);
  InitRunAndExpectSuccess(kIpV4Endpoint1, service_endpoint,
                          /*expect_sync_result=*/false);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  EXPECT_EQ(kDnsAliases, test_delegate_->socket()->GetDnsAliases());
}

TEST_F(TcpConnectJobTest, ConnectionErrorSyncDnsAsyncConnect) {
  host_resolver_.ConfigureDefaultResolution()
      .add_endpoint(CreateServiceEndpoint({kIpV4Endpoint1}))
      .CompleteStartSynchronously(OK);
  AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV4Endpoint1);
  InitRunAndExpectError(ERR_FAILED, /*expect_sync_result=*/false);
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
}

TEST_F(TcpConnectJobTest, ConnectionSuccessAsyncDnsSyncConnect) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  host_resolver_.AddFakeRequest()
      ->add_endpoint(service_endpoint)
      .set_aliases(kDnsAliases)
      .CompleteStartAsynchronously(OK);
  AddConnect(MockConnect(SYNCHRONOUS, OK), kIpV4Endpoint1);
  InitRunAndExpectSuccess(kIpV4Endpoint1, service_endpoint,
                          /*expect_sync_result=*/false);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  EXPECT_EQ(kDnsAliases, test_delegate_->socket()->GetDnsAliases());
}

TEST_F(TcpConnectJobTest, ConnectionErrorAsyncDnsSyncConnect) {
  host_resolver_.AddFakeRequest()
      ->add_endpoint(CreateServiceEndpoint({kIpV4Endpoint1}))
      .CompleteStartAsynchronously(OK);
  AddConnect(MockConnect(SYNCHRONOUS, ERR_FAILED), kIpV4Endpoint1);
  InitRunAndExpectError(ERR_FAILED, /*expect_sync_result=*/false);
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
}

TEST_F(TcpConnectJobTest, ConnectionSuccessAsyncDnsAsyncConnect) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  host_resolver_.AddFakeRequest()
      ->add_endpoint(service_endpoint)
      .set_aliases(kDnsAliases)
      .CompleteStartAsynchronously(OK);
  AddConnect(MockConnect(ASYNC, OK), kIpV4Endpoint1);
  InitRunAndExpectSuccess(kIpV4Endpoint1, service_endpoint,
                          /*expect_sync_result=*/false);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  EXPECT_EQ(kDnsAliases, test_delegate_->socket()->GetDnsAliases());
}

TEST_F(TcpConnectJobTest, ConnectionErrorAsyncDnsAsyncConnect) {
  host_resolver_.AddFakeRequest()
      ->add_endpoint(CreateServiceEndpoint({kIpV4Endpoint1}))
      .CompleteStartAsynchronously(OK);
  AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV4Endpoint1);
  InitRunAndExpectError(ERR_FAILED, /*expect_sync_result=*/false);
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
}

// Test the case where DNS never completes, but is crypto ready and provides an
// IP address, which should be enough for the job to connect and return a
// result.
TEST_F(TcpConnectJobTest, ConnectionSuccessPartialDns) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  for (auto connect : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(connect);
    auto request = host_resolver_.AddFakeRequest();
    request->set_start_callback(base::BindLambdaForTesting([&]() {
      request->set_crypto_ready(true)
          .add_endpoint(service_endpoint)
          .set_aliases(kDnsAliases)
          .CallOnServiceEndpointsUpdated();
    }));
    AddConnect(MockConnect(connect, OK), kIpV4Endpoint1);
    InitRunAndExpectSuccess(kIpV4Endpoint1, service_endpoint,
                            /*expect_sync_result=*/false);
    EXPECT_TRUE(connect_job_->HasEstablishedConnection());
    EXPECT_EQ(kDnsAliases, test_delegate_->socket()->GetDnsAliases());
  }
}

// Test the case where DNS provides an IP, and we manage to connect to it,
// before crypto ready is received. This covers both the case the crypto ready
// state is learned about when the DNS request completes, and when it is learned
// about during an update, and the DNS request never completes.
//
// Note that there are DNS error tests that test the case of DNS errors after
// connect and before crypto ready.
TEST_F(TcpConnectJobTest, ConnectionSuccessThenCryptoReady) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  for (bool dns_completes : {true, false}) {
    SCOPED_TRACE(dns_completes);
    for (auto connect : {SYNCHRONOUS, ASYNC}) {
      SCOPED_TRACE(connect);
      MockConnectCompleter connect_completer;
      AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);
      auto request = host_resolver_.AddFakeRequest();
      request->set_start_callback(base::BindLambdaForTesting([&]() {
        request->add_endpoint(service_endpoint).CallOnServiceEndpointsUpdated();
        EXPECT_FALSE(connect_job_->HasEstablishedConnection());
        connect_completer.WaitForConnectAndComplete(OK);
        EXPECT_TRUE(connect_job_->HasEstablishedConnection());
        if (dns_completes) {
          request->set_crypto_ready(true)
              .set_aliases(kDnsAliases)
              .CallOnServiceEndpointsUpdated();
        } else {
          request->set_crypto_ready(true)
              .set_aliases(kDnsAliases)
              .CallOnServiceEndpointRequestFinished(OK);
        }
      }));
      InitRunAndExpectSuccess(kIpV4Endpoint1, service_endpoint,
                              /*expect_sync_result=*/false);
      EXPECT_EQ(kDnsAliases, test_delegate_->socket()->GetDnsAliases());
      EXPECT_TRUE(connect_job_->HasEstablishedConnection());
    }
  }
}

// Just like the above test, but with an extra ServiceEndpointsUpdated() even
// before crypto ready is set. The request should not complete until crypto
// ready is set.
TEST_F(TcpConnectJobTest, ConnectionSuccessThenCryptoReady2) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  for (bool dns_completes : {true, false}) {
    SCOPED_TRACE(dns_completes);
    for (auto connect : {SYNCHRONOUS, ASYNC}) {
      SCOPED_TRACE(connect);
      MockConnectCompleter connect_completer;
      AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);
      auto request = host_resolver_.AddFakeRequest();
      request->set_start_callback(base::BindLambdaForTesting([&]() {
        request->add_endpoint(service_endpoint).CallOnServiceEndpointsUpdated();
        EXPECT_FALSE(connect_job_->HasEstablishedConnection());
        connect_completer.WaitForConnectAndComplete(OK);
        EXPECT_TRUE(connect_job_->HasEstablishedConnection());

        // A superfluous OnServiceEndpointsUpdated() event. We could add more
        // endpoints, but it's not needed for this test. It should not cause the
        // request to complete. Spin the message loop by advancing time to make
        // sure there are no pending completion events.
        request->CallOnServiceEndpointsUpdated();
        FastForwardBy(base::Milliseconds(1));
        EXPECT_FALSE(test_delegate_->has_result());

        if (dns_completes) {
          request->set_crypto_ready(true).CallOnServiceEndpointsUpdated();
        } else {
          request->set_crypto_ready(true).CallOnServiceEndpointRequestFinished(
              OK);
        }
      }));
      InitRunAndExpectSuccess(kIpV4Endpoint1, service_endpoint,
                              /*expect_sync_result=*/false);
    }
  }
}

// Test the case that the first IP fails and there are two IPv6 IPs. This tests
// tries all sync/async combinations for the DNS lookup and both connection
// attempts, as well as success/failure for the second connection attempt.
TEST_F(TcpConnectJobTest, TwoIpsFirstIpFails) {
  const auto service_endpoint =
      CreateServiceEndpoint({kIpV6Endpoint1, kIpV6Endpoint2});
  for (bool dns_sync : {false, true}) {
    for (bool first_connect_sync : {false, true}) {
      for (bool second_connect_sync : {false, true}) {
        for (Error second_connect_result : {OK, ERR_UNEXPECTED}) {
          bool everything_sync =
              dns_sync && first_connect_sync && second_connect_sync;
          auto dns_request = host_resolver_.AddFakeRequest();
          dns_request->add_endpoint(service_endpoint);
          if (dns_sync) {
            dns_request->CompleteStartSynchronously(OK);
          } else {
            dns_request->CompleteStartAsynchronously(OK);
          }
          // The first endpoint fails.
          AddConnect(
              MockConnect(first_connect_sync ? SYNCHRONOUS : ASYNC, ERR_FAILED),
              kIpV6Endpoint1);
          // The second endpoint may succeed or fail.
          AddConnect(MockConnect(second_connect_sync ? SYNCHRONOUS : ASYNC,
                                 second_connect_result),
                     kIpV6Endpoint2);
          if (second_connect_result == OK) {
            InitRunAndExpectSuccess(kIpV6Endpoint2, service_endpoint,
                                    /*expect_sync_result=*/everything_sync);
          } else {
            InitRunAndExpectError(second_connect_result,
                                  /*expect_sync_result=*/everything_sync);
          }
          ASSERT_TRUE(client_socket_factory_.AllDataProvidersUsed());
        }
      }
    }
  }
}

// Test that ERR_NETWORK_IO_SUSPENDED fails a job instantly, preventing it from
// trying any other IPs.
TEST_F(TcpConnectJobTest, NetworkIoSuspendedFailsInstantly) {
  const auto service_endpoint =
      CreateServiceEndpoint({kIpV6Endpoint1, kIpV6Endpoint2});
  for (bool dns_sync : {false, true}) {
    for (bool connect_sync : {false, true}) {
      bool everything_sync = dns_sync && connect_sync;
      auto dns_request = host_resolver_.AddFakeRequest();
      dns_request->add_endpoint(CreateServiceEndpoint(
          {kIpV4Endpoint1, kIpV6Endpoint1, kIpV6Endpoint2}));
      dns_request->add_endpoint(
          CreateServiceEndpoint({kIpV4Endpoint2, kIpV6Endpoint3}));
      if (dns_sync) {
        dns_request->CompleteStartSynchronously(OK);
      } else {
        dns_request->CompleteStartAsynchronously(OK);
      }
      AddConnect(MockConnect(connect_sync ? SYNCHRONOUS : ASYNC,
                             ERR_NETWORK_IO_SUSPENDED),
                 kIpV6Endpoint1);
      InitRunAndExpectError(ERR_NETWORK_IO_SUSPENDED,
                            /*expect_sync_result=*/everything_sync);
    }
  }
}

// Test which IP is used first when all DNS results come in at once. In all
// cases, the first attempt to connect (if there is one) succeeds. This test
// does not modify anything other than the protocol of the destination and the
// DNS result (e.g., no modified ALPNs, or disabling of ech).
TEST_F(TcpConnectJobTest, FirstAttemptedIPEndPoint) {
  // Each test case is run three times with three different destinations,
  // possibly expecting different behavior.
  const std::array<TransportSocketParams::Endpoint, 3> kDestinations = {
      url::SchemeHostPort(url::kHttpsScheme, kHostName, 443),
      url::SchemeHostPort(url::kHttpScheme, kHostName, 443),
      HostPortPair(kHostName, 443),
  };

  struct ExpectedEndpoints {
    IPEndPoint ip_endpoint;
    // The index in the associated `service_endpoints` vector of the
    // ServiceEndpoint that is expected to be returned by GetServiceEndpoint().
    int service_endpoint_index;
  };

  const struct {
    std::string_view test_name;
    std::vector<ServiceEndpoint> service_endpoints;
    // The endpoint we expect to connect to, based on scheme the destination
    // uses. The order is HTTPS, HTTP, and then schemeless, matching the order
    // of `kDestinations`.
    std::array<std::optional<ExpectedEndpoints>, 3> expected_endpoints;
  } kTestCases[] = {
      {"Single IPv4",
       {CreateServiceEndpoint({kIpV4Endpoint1})},
       {ExpectedEndpoints{kIpV4Endpoint1, 0} /*https*/,
        ExpectedEndpoints{kIpV4Endpoint1, 0} /*http*/,
        ExpectedEndpoints{kIpV4Endpoint1, 0} /*no scheme*/}},

      {"Two IPv4",
       {CreateServiceEndpoint({kIpV4Endpoint2, kIpV4Endpoint1})},
       {ExpectedEndpoints{kIpV4Endpoint2, 0} /*https*/,
        ExpectedEndpoints{kIpV4Endpoint2, 0} /*http*/,
        ExpectedEndpoints{kIpV4Endpoint2, 0} /*no scheme*/}},

      {"Single IPv6",
       {CreateServiceEndpoint({kIpV6Endpoint1})},
       {ExpectedEndpoints{kIpV6Endpoint1, 0} /*https*/,
        ExpectedEndpoints{kIpV6Endpoint1, 0} /*http*/,
        ExpectedEndpoints{kIpV6Endpoint1, 0} /*no scheme*/}},

      {"Two IPv6",
       {CreateServiceEndpoint({kIpV6Endpoint2, kIpV6Endpoint1})},
       {ExpectedEndpoints{kIpV6Endpoint2, 0} /*https*/,
        ExpectedEndpoints{kIpV6Endpoint2, 0} /*http*/,
        ExpectedEndpoints{kIpV6Endpoint2, 0} /*no scheme*/}},

      {"IPv4 and IPv6",
       {CreateServiceEndpoint({kIpV4Endpoint1, kIpV6Endpoint1})},
       {ExpectedEndpoints{kIpV6Endpoint1, 0} /*https*/,
        ExpectedEndpoints{kIpV6Endpoint1, 0} /*http*/,
        ExpectedEndpoints{kIpV6Endpoint1, 0} /*no scheme*/}},

      {"IPv4 and IPv6, different ServiceEndpoints",
       {CreateServiceEndpoint({kIpV4Endpoint1}),
        CreateServiceEndpoint({kIpV6Endpoint1})},
       {ExpectedEndpoints{kIpV4Endpoint1, 0} /*https*/,
        ExpectedEndpoints{kIpV4Endpoint1, 0} /*http*/,
        ExpectedEndpoints{kIpV4Endpoint1, 0} /*no scheme*/}},

      {"HTTP/1.x ALPN",
       {CreateServiceEndpoint({kIpV6Endpoint1}, {"http/1.1"})},
       {ExpectedEndpoints{kIpV6Endpoint1, 0} /*https*/, std::nullopt /*http*/,
        std::nullopt /*no scheme*/}},

      {"HTTP/1.x ALPN with AAAA fallback (different destination)",
       {CreateServiceEndpoint({kIpV6Endpoint1}, {"http/1.1"}),
        CreateServiceEndpoint({kIpV6Endpoint2})},
       {ExpectedEndpoints{kIpV6Endpoint1, 0} /*https*/,
        ExpectedEndpoints{kIpV6Endpoint2, 1} /*http*/,
        ExpectedEndpoints{kIpV6Endpoint2, 1} /*no scheme*/}},

      {"H2 ALPN",
       {CreateServiceEndpoint({kIpV6Endpoint1}, {"h2"})},
       {ExpectedEndpoints{kIpV6Endpoint1, 0} /*https*/, std::nullopt /*http*/,
        std::nullopt /*no scheme*/}},

      {"H2 ALPN with AAAA fallback (different destination)",
       {CreateServiceEndpoint({kIpV6Endpoint1}, {"h2"}),
        CreateServiceEndpoint({kIpV6Endpoint2})},
       {ExpectedEndpoints{kIpV6Endpoint1, 0} /*https*/,
        ExpectedEndpoints{kIpV6Endpoint2, 1} /*http*/,
        ExpectedEndpoints{kIpV6Endpoint2, 1} /*no scheme*/}},

      {"H2 ALPN with AAAA fallback (same destination)",
       {CreateServiceEndpoint({kIpV6Endpoint1}, {"h2"}),
        CreateServiceEndpoint({kIpV6Endpoint1})},
       {ExpectedEndpoints{kIpV6Endpoint1, 0} /*https*/,
        ExpectedEndpoints{kIpV6Endpoint1, 1} /*http*/,
        ExpectedEndpoints{kIpV6Endpoint1, 1} /*no scheme*/}},

      {"H3 ALPN",
       {CreateServiceEndpoint({kIpV6Endpoint1}, {"h3"})},
       {std::nullopt /*https*/, std::nullopt /*http*/,
        std::nullopt /*no scheme*/}},

      {"H3 ALPN with AAAA fallback (different destination)",
       {CreateServiceEndpoint({kIpV6Endpoint1}, {"h3"}),
        CreateServiceEndpoint({kIpV6Endpoint2})},
       {ExpectedEndpoints{kIpV6Endpoint2, 1} /*https*/,
        ExpectedEndpoints{kIpV6Endpoint2, 1} /*http*/,
        ExpectedEndpoints{kIpV6Endpoint2, 1} /*no scheme*/}},

      {"H3 ALPN with AAAA fallback (same destination)",
       {CreateServiceEndpoint({kIpV6Endpoint1}, {"h3"}),
        CreateServiceEndpoint({kIpV6Endpoint1})},
       {ExpectedEndpoints{kIpV6Endpoint1, 1} /*https*/,
        ExpectedEndpoints{kIpV6Endpoint1, 1} /*http*/,
        ExpectedEndpoints{kIpV6Endpoint1, 1} /*no scheme*/}},

      // In this case, `svcb_optional_` should be set to false, so the HTTPS
      // attempt will reject all endpoints.
      {"H3 ALPN with ech AAAA fallback (different destination)",
       {CreateServiceEndpoint({kIpV6Endpoint1}, {"h3"}, /*ech=*/true),
        CreateServiceEndpoint({kIpV6Endpoint2})},
       {std::nullopt /*https*/, ExpectedEndpoints{kIpV6Endpoint2, 1} /*http*/,
        ExpectedEndpoints{kIpV6Endpoint2, 1} /*no scheme*/}},

      // In this case, `svcb_optional_` should be set to true, since not all of
      // the alternative endpoints have ECH set, so the HTTPS attempt will not
      // reject the AAAA endpoints.
      {"H3 ALPN with partial ech AAAA fallback (different destination)",
       {CreateServiceEndpoint({kIpV6Endpoint1}, {"h3"}, /*ech=*/true),
        CreateServiceEndpoint({kIpV6Endpoint2}, {"h3"}, /*ech=*/false),
        CreateServiceEndpoint({kIpV4Endpoint1})},
       {ExpectedEndpoints{kIpV4Endpoint1, 2} /*http*/,
        ExpectedEndpoints{kIpV4Endpoint1, 2} /*http*/,
        ExpectedEndpoints{kIpV4Endpoint1, 2} /*no scheme*/}},

      // If there are supported ALPNs as well as unsupported ones (h3), the
      // supported ones take precedence.
      {"Multiple ALPNs with AAAA fallback (different destination)",
       {CreateServiceEndpoint({kIpV6Endpoint1}, {"http/1.1", "h2", "h3"}),
        CreateServiceEndpoint({kIpV6Endpoint2})},
       {ExpectedEndpoints{kIpV6Endpoint1, 0} /*https*/,
        ExpectedEndpoints{kIpV6Endpoint2, 1} /*http*/,
        ExpectedEndpoints{kIpV6Endpoint2, 1} /*no scheme*/}},
  };

  for (size_t i = 0; i < kDestinations.size(); ++i) {
    destination_ = kDestinations[i];
    std::string scheme;
    const url::SchemeHostPort* scheme_host_port =
        std::get_if<url::SchemeHostPort>(&destination_);
    if (scheme_host_port) {
      scheme = scheme_host_port->scheme();
    } else {
      scheme = "no scheme";
    }

    for (const auto& test : kTestCases) {
      SCOPED_TRACE(base::StrCat({test.test_name, ": ", scheme}));
      host_resolver_.AddFakeRequest()
          ->set_endpoints(test.service_endpoints)
          .CompleteStartAsynchronously(OK);

      if (test.expected_endpoints[i]) {
        AddConnect(MockConnect(ASYNC, OK),
                   test.expected_endpoints[i]->ip_endpoint);
        InitRunAndExpectSuccess(
            test.expected_endpoints[i]->ip_endpoint,
            test.service_endpoints[test.expected_endpoints[i]
                                       ->service_endpoint_index],
            /*expect_sync_result=*/false);
      } else {
        InitRunAndExpectError(ERR_NAME_NOT_RESOLVED,
                              /*expect_sync_result=*/false);
      }
    }
  }
}

// Test the case that H2 is disabled for a request. This only tests the HTTPS
// case, as `supported_alpns_` is expected to be empty for HTTP and schemeless
// requests.
TEST_F(TcpConnectJobTest, H2Disabled) {
  supported_alpns_ = {"http/1.1"};

  // IPs with H2 alpns are rejected.
  host_resolver_.ConfigureDefaultResolution()
      .add_endpoint(CreateServiceEndpoint({kIpV4Endpoint1}, {"h2"}))
      .CompleteStartSynchronously(OK);
  InitRunAndExpectError(ERR_NAME_NOT_RESOLVED, /*expect_sync_result=*/true);

  // Ech will still disable non-svcb records.
  host_resolver_.ConfigureDefaultResolution()
      .add_endpoint(
          CreateServiceEndpoint({kIpV4Endpoint1}, {"h2"}, /*ech=*/true))
      .add_endpoint(CreateServiceEndpoint({kIpV6Endpoint1}))
      .CompleteStartSynchronously(OK);
  InitRunAndExpectError(ERR_NAME_NOT_RESOLVED, /*expect_sync_result=*/true);

  // Fallback to non-svcb records still happens without ECH.
  host_resolver_.ConfigureDefaultResolution()
      .add_endpoint(CreateServiceEndpoint({kIpV4Endpoint1}, {"h2"}))
      .add_endpoint(CreateServiceEndpoint({kIpV6Endpoint1}))
      .CompleteStartSynchronously(OK);
  AddConnect(MockConnect(SYNCHRONOUS, OK), kIpV6Endpoint1);
  InitRunAndExpectSuccess(kIpV6Endpoint1,
                          CreateServiceEndpoint({kIpV6Endpoint1}),
                          /*expect_sync_result=*/true);
}

// Test that disabling ECH makes `svcb_optional_` true.
TEST_F(TcpConnectJobTest, EchDisabled) {
  SSLContextConfig ssl_context_config;
  ssl_context_config.ech_enabled = false;
  ssl_config_service_.UpdateSSLConfigAndNotify(ssl_context_config);

  // IPs with H3 alpns still rejected.
  host_resolver_.ConfigureDefaultResolution()
      .add_endpoint(CreateServiceEndpoint({kIpV4Endpoint1}, {"h3"}))
      .CompleteStartSynchronously(OK);
  InitRunAndExpectError(ERR_NAME_NOT_RESOLVED, /*expect_sync_result=*/true);

  // Ech will no longer disable non-svcb records.
  host_resolver_.ConfigureDefaultResolution()
      .add_endpoint(
          CreateServiceEndpoint({kIpV4Endpoint1}, {"h3"}, /*ech=*/true))
      .add_endpoint(CreateServiceEndpoint({kIpV6Endpoint1}))
      .CompleteStartSynchronously(OK);
  AddConnect(MockConnect(ASYNC, OK), kIpV6Endpoint1);
  InitRunAndExpectSuccess(kIpV6Endpoint1,
                          CreateServiceEndpoint({kIpV6Endpoint1}),
                          /*expect_sync_result=*/false);
}

// Test that ServiceEndpoints are tried in order, with IPv6 IPs first, and no
// IPs are retried. All ServiceEndpoints are received at once.
TEST_F(TcpConnectJobTest, FallbackOrderOneConnectorNoAlpn) {
  std::vector<ServiceEndpoint> service_endpoints{
      // This will result in trying IPs in this order, since IPv4 and IPv6
      // requests are alternated:
      //   kIpV6Endpoint1, kIpV4Endpoint1, kIpV6Endpoint2, kIpV4Endpoint2
      CreateServiceEndpoint(
          {kIpV4Endpoint1, kIpV4Endpoint2, kIpV6Endpoint1, kIpV6Endpoint2}),
      // Of these, only kIpV6Endpoint3 is new, so only it should be tried.
      CreateServiceEndpoint({kIpV4Endpoint1, kIpV6Endpoint3, kIpV6Endpoint1}),
      // These have been tried before, so should be ignored.
      CreateServiceEndpoint({kIpV6Endpoint2, kIpV6Endpoint3}),
      // There are two new endpoints here. Since the last one tried was IPv6,
      // kIpV4Endpoint3 should be tried, and then kIpV6Endpoint4.
      CreateServiceEndpoint({kIpV6Endpoint1, kIpV4Endpoint3, kIpV6Endpoint4}),
  };

  for (Error final_connect_result : {OK, ERR_UNEXPECTED}) {
    SCOPED_TRACE(final_connect_result);
    host_resolver_.AddFakeRequest()
        ->set_endpoints(service_endpoints)
        .CompleteStartAsynchronously(OK);
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV6Endpoint1);
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV4Endpoint1);
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV6Endpoint2);
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV4Endpoint2);
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV6Endpoint3);
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV4Endpoint3);
    AddConnect(MockConnect(ASYNC, final_connect_result), kIpV6Endpoint4);
    if (final_connect_result == OK) {
      InitRunAndExpectSuccess(kIpV6Endpoint4, service_endpoints.back(),
                              /*expect_sync_result=*/false);
    } else {
      InitRunAndExpectError(final_connect_result,
                            /*expect_sync_result=*/false);
    }
    ASSERT_TRUE(client_socket_factory_.AllDataProvidersUsed());
  }
}

// Test that ServiceEndpoints are tried in order, with IPv6 IPs first, and no
// IPs are retried. All ServiceEndpoints are received at once. Some endpoints
// have alt service information.
TEST_F(TcpConnectJobTest, FallbackOrderOneConnectorWithAlpn) {
  std::vector<ServiceEndpoint> service_endpoints{
      // This will result in trying IPs in this order, since IPv4 and IPv6
      // requests are alternated:
      //   kIpV6Endpoint1, kIpV4Endpoint1, kIpV6Endpoint2, kIpV4Endpoint2
      CreateServiceEndpoint(
          {kIpV4Endpoint1, kIpV4Endpoint2, kIpV6Endpoint1, kIpV6Endpoint2},
          {"h2"}),
      // One of these is new, the others are not, but since the ALPN is h3, they
      // should not be tried.
      CreateServiceEndpoint({kIpV4Endpoint1, kIpV6Endpoint3, kIpV6Endpoint1},
                            {"h3"}),
      // There are two new endpoints here. Since the last one tried was IPv4,
      // kIpV6Endpoint4 should be tried, and then kIpV4Endpoint4.
      CreateServiceEndpoint({kIpV6Endpoint1, kIpV4Endpoint4, kIpV6Endpoint4}),
      // kIpV6Endpoint2 was already tried, so only `kIpV4Endpoint3` should be
      // tried here.
      CreateServiceEndpoint({kIpV6Endpoint2, kIpV4Endpoint3}),
  };

  for (Error final_connect_result : {OK, ERR_UNEXPECTED}) {
    SCOPED_TRACE(final_connect_result);
    host_resolver_.AddFakeRequest()
        ->set_endpoints(service_endpoints)
        .CompleteStartAsynchronously(OK);
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV6Endpoint1);
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV4Endpoint1);
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV6Endpoint2);
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV4Endpoint2);
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV6Endpoint4);
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV4Endpoint4);
    AddConnect(MockConnect(ASYNC, final_connect_result), kIpV4Endpoint3);
    if (final_connect_result == OK) {
      InitRunAndExpectSuccess(kIpV4Endpoint3, service_endpoints.back(),
                              /*expect_sync_result=*/false);
    } else {
      InitRunAndExpectError(final_connect_result,
                            /*expect_sync_result=*/false);
    }
    ASSERT_TRUE(client_socket_factory_.AllDataProvidersUsed());
  }
}

// Test the case where we only learn that we can't use ServiceEndpoints without
// alt service information after we start connecting, in the case there are
// ultimately no usable IP endpoints.
TEST_F(TcpConnectJobTest, CryptoReadyAfterConnectStartNoUsableIps) {
  ServiceEndpoint service_endpoint_https =
      CreateServiceEndpoint({kIpV6Endpoint1}, {"h3"}, /*ech=*/true);
  ServiceEndpoint service_endpoint_aaaa =
      CreateServiceEndpoint({kIpV6Endpoint2});

  for (bool connect_complete_before_crypto_ready : {false, true}) {
    SCOPED_TRACE(connect_complete_before_crypto_ready);

    auto request = host_resolver_.AddFakeRequest();
    // AAAA results arrive first.
    request->set_start_callback(base::BindLambdaForTesting([&]() {
      request->set_endpoints({service_endpoint_aaaa})
          .CallOnServiceEndpointsUpdated();
    }));

    // Connect to the IP from the AAAA record.
    MockConnectCompleter connect_completer;
    AddConnect(MockConnect(&connect_completer), kIpV6Endpoint2);

    EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

    connect_completer.WaitForConnect();
    // Even if the connection completes, without crypto ready being set, the
    // ConnectJob won't complete.
    if (connect_complete_before_crypto_ready) {
      EXPECT_FALSE(connect_job_->HasEstablishedConnection());
      connect_completer.Complete(OK);
      EXPECT_TRUE(connect_job_->HasEstablishedConnection());
    }

    // DNS request completes, at which point we learn that no IPs are usable.
    // While it shouldn't matter for this test, rearrange endpoints so the HTTPS
    // endpoints are first, to reflect real behavior.
    request->set_endpoints({service_endpoint_https, service_endpoint_aaaa})
        .CallOnServiceEndpointRequestFinished(OK);

    // Currently, we wait for complete before checking if the destination is
    // usable again.
    if (!connect_complete_before_crypto_ready) {
      EXPECT_FALSE(connect_job_->HasEstablishedConnection());
      connect_completer.Complete(OK);
      EXPECT_TRUE(connect_job_->HasEstablishedConnection());
    }

    // It is a little weird to fail with this error when we actually did get
    // some IP addresses, but this is what we currently do.
    EXPECT_THAT(test_delegate_->WaitForResult(),
                IsError(ERR_NAME_NOT_RESOLVED));
    EXPECT_TRUE(connect_job_->HasEstablishedConnection());
    ASSERT_TRUE(client_socket_factory_.AllDataProvidersUsed());
  }
}

// Test the case where we only learn that we can't use ServiceEndpoints without
// alt service information after we start connecting, in the case there is a
// different usable IP endpoint. The first connection should be dropped, and the
// new IP used.
TEST_F(TcpConnectJobTest, CryptoReadyAfterConnectStartDifferentUsableIp) {
  ServiceEndpoint service_endpoint_https =
      CreateServiceEndpoint({kIpV6Endpoint1}, {"h2"}, /*ech=*/true);
  ServiceEndpoint service_endpoint_aaaa =
      CreateServiceEndpoint({kIpV6Endpoint2});

  for (bool connect_complete_before_crypto_ready : {false, true}) {
    SCOPED_TRACE(connect_complete_before_crypto_ready);

    auto request = host_resolver_.AddFakeRequest();
    // AAAA results arrive first.
    request->set_start_callback(base::BindLambdaForTesting([&]() {
      request->set_endpoints({service_endpoint_aaaa})
          .CallOnServiceEndpointsUpdated();
    }));

    // Connect to the IP from the AAAA record.
    MockConnectCompleter connect_completer;
    AddConnect(MockConnect(&connect_completer), kIpV6Endpoint2);
    AddConnect(MockConnect(ASYNC, OK), kIpV6Endpoint1);

    EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

    connect_completer.WaitForConnect();
    // Even if the connection completes, without crypto ready being set, the
    // ConnectJob won't complete.
    if (connect_complete_before_crypto_ready) {
      EXPECT_FALSE(connect_job_->HasEstablishedConnection());
      connect_completer.Complete(OK);
      EXPECT_TRUE(connect_job_->HasEstablishedConnection());
    }

    // DNS request completes, at which point we learn that no IPs are usable.
    // While it shouldn't matter for this test, rearrange endpoints so the HTTPS
    // endpoints are first, to reflect real behavior.
    request->set_endpoints({service_endpoint_https, service_endpoint_aaaa})
        .CallOnServiceEndpointRequestFinished(OK);

    // Currently, we wait for complete before checking if the destination is
    // usable again.
    if (!connect_complete_before_crypto_ready) {
      EXPECT_FALSE(connect_job_->HasEstablishedConnection());
      connect_completer.Complete(OK);
      EXPECT_TRUE(connect_job_->HasEstablishedConnection());
    }

    EXPECT_THAT(test_delegate_->WaitForResult(), IsOk());
    CheckConnection(kIpV6Endpoint1, service_endpoint_https);
    EXPECT_TRUE(connect_job_->HasEstablishedConnection());
    ASSERT_TRUE(client_socket_factory_.AllDataProvidersUsed());
  }
}

// Test the case where we only learn that we can't use ServiceEndpoints without
// alt service information after we start connecting, in the case there is a
// different usable service endpoint that has same the IP endpoint we were
// already connecting to. The connection should be used, and the usable service
// endpoint returned rather than the one that triggered the connection attempt.
TEST_F(TcpConnectJobTest, CryptoReadyAfterConnectStartSameUsableIp) {
  ServiceEndpoint service_endpoint_https =
      CreateServiceEndpoint({kIpV6Endpoint1}, {"h2"}, /*ech=*/true);
  ServiceEndpoint service_endpoint_aaaa =
      CreateServiceEndpoint({kIpV6Endpoint1});

  for (bool connect_complete_before_crypto_ready : {false, true}) {
    SCOPED_TRACE(connect_complete_before_crypto_ready);

    auto request = host_resolver_.AddFakeRequest();
    // AAAA results arrive first.
    request->set_start_callback(base::BindLambdaForTesting([&]() {
      request->set_endpoints({service_endpoint_aaaa})
          .CallOnServiceEndpointsUpdated();
    }));

    // Connect to the IP from the AAAA record.
    MockConnectCompleter connect_completer;
    AddConnect(MockConnect(&connect_completer), kIpV6Endpoint1);

    EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

    connect_completer.WaitForConnect();
    // Even if the connection completes, without crypto ready being set, the
    // ConnectJob won't complete.
    if (connect_complete_before_crypto_ready) {
      EXPECT_FALSE(connect_job_->HasEstablishedConnection());
      connect_completer.Complete(OK);
      EXPECT_TRUE(connect_job_->HasEstablishedConnection());
    }

    // DNS request completes, at which point we learn that no IPs are usable.
    // While it shouldn't matter for this test, rearrange endpoints so the HTTPS
    // endpoints are first, to reflect real behavior.
    request->set_endpoints({service_endpoint_https, service_endpoint_aaaa})
        .CallOnServiceEndpointRequestFinished(OK);

    if (!connect_complete_before_crypto_ready) {
      EXPECT_FALSE(connect_job_->HasEstablishedConnection());
      connect_completer.Complete(OK);
      EXPECT_TRUE(connect_job_->HasEstablishedConnection());
    }

    EXPECT_THAT(test_delegate_->WaitForResult(), IsOk());
    CheckConnection(kIpV6Endpoint1, service_endpoint_https);
    EXPECT_TRUE(connect_job_->HasEstablishedConnection());
    ASSERT_TRUE(client_socket_factory_.AllDataProvidersUsed());
  }
}

// Test the case where we only learn that we can use ServiceEndpoints without
// alt service information after we start connecting, but where there's a
// matching higher priority ServiceEndpoint. In this case, the connection is
// used, but the higher priority ServiceEndpoint should be reported as being
// used.
TEST_F(TcpConnectJobTest, CryptoReadyAfterConnectStartSameUsableIpNoEch) {
  ServiceEndpoint service_endpoint_https =
      CreateServiceEndpoint({kIpV6Endpoint1}, {"h2"}, /*ech=*/false);
  ServiceEndpoint service_endpoint_aaaa =
      CreateServiceEndpoint({kIpV6Endpoint1});

  for (bool connect_complete_before_crypto_ready : {false, true}) {
    SCOPED_TRACE(connect_complete_before_crypto_ready);

    auto request = host_resolver_.AddFakeRequest();
    // AAAA results arrive first.
    request->set_start_callback(base::BindLambdaForTesting([&]() {
      request->set_endpoints({service_endpoint_aaaa})
          .CallOnServiceEndpointsUpdated();
    }));

    // Connect to the IP from the AAAA record.
    MockConnectCompleter connect_completer;
    AddConnect(MockConnect(&connect_completer), kIpV6Endpoint1);

    EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

    connect_completer.WaitForConnect();
    // Even if the connection completes, without crypto ready being set, the
    // ConnectJob won't complete.
    if (connect_complete_before_crypto_ready) {
      EXPECT_FALSE(connect_job_->HasEstablishedConnection());
      connect_completer.Complete(OK);
      EXPECT_TRUE(connect_job_->HasEstablishedConnection());
    }

    // DNS request completes, at which point we learn that no IPs are usable.
    // While it shouldn't matter for this test, rearrange endpoints so the HTTPS
    // endpoints are first, to reflect real behavior.
    request->set_endpoints({service_endpoint_https, service_endpoint_aaaa})
        .CallOnServiceEndpointRequestFinished(OK);

    if (!connect_complete_before_crypto_ready) {
      EXPECT_FALSE(connect_job_->HasEstablishedConnection());
      connect_completer.Complete(OK);
      EXPECT_TRUE(connect_job_->HasEstablishedConnection());
    }

    EXPECT_THAT(test_delegate_->WaitForResult(), IsOk());
    CheckConnection(kIpV6Endpoint1, service_endpoint_https);
    EXPECT_TRUE(connect_job_->HasEstablishedConnection());
    ASSERT_TRUE(client_socket_factory_.AllDataProvidersUsed());
  }
}

// Test the case where the entire ConnectJob stalls after the only IP it has
// fails. Then another IP comes in and it resumes.
TEST_F(TcpConnectJobTest, ConnectsStallForDns) {
  ServiceEndpoint service_endpoint1 = CreateServiceEndpoint({kIpV6Endpoint1});
  ServiceEndpoint service_endpoint2 = CreateServiceEndpoint({kIpV4Endpoint1});

  ServiceEndpoint service_endpoint_quic =
      CreateServiceEndpoint({kIpV6Endpoint2}, {"h3"});

  // Whether an extra update with a QUIC-only endpoint should be included.
  for (bool add_superfluous_endpoint : {false, true}) {
    SCOPED_TRACE(add_superfluous_endpoint);
    for (Error final_connect_result : {ERR_UNEXPECTED, OK}) {
      SCOPED_TRACE(final_connect_result);

      auto request = host_resolver_.AddFakeRequest();
      // AAAA results arrive first.
      request->set_start_callback(base::BindLambdaForTesting([&]() {
        request->set_endpoints({service_endpoint1})
            .CallOnServiceEndpointsUpdated();
      }));

      MockConnectCompleter connect_completer1;
      AddConnect(MockConnect(&connect_completer1), kIpV6Endpoint1);
      MockConnectCompleter connect_completer2;
      AddConnect(MockConnect(&connect_completer2), kIpV4Endpoint1);

      EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

      // Fail the first request. No second request should be made yet, since the
      // second ServiceEndpoint hasn't been received yet.
      EXPECT_FALSE(connect_job_->HasEstablishedConnection());
      connect_completer1.WaitForConnectAndComplete(ERR_FAILED);
      FastForwardBy(base::Milliseconds(1));
      EXPECT_FALSE(test_delegate_->has_result());
      EXPECT_FALSE(connect_job_->HasEstablishedConnection());

      // Add an extra endpoint that should not resume connecting, if needed.
      if (add_superfluous_endpoint) {
        request->set_endpoints({service_endpoint_quic, service_endpoint1})
            .CallOnServiceEndpointsUpdated();
        FastForwardBy(base::Milliseconds(1));
        EXPECT_FALSE(test_delegate_->has_result());
      }

      // Second viable endpoint is received and the DNS request completes, which
      // should trigger the final connection attempt.
      request->add_endpoint(service_endpoint2)
          .CallOnServiceEndpointRequestFinished(OK);
      EXPECT_FALSE(connect_job_->HasEstablishedConnection());
      connect_completer2.WaitForConnectAndComplete(final_connect_result);
      EXPECT_EQ(connect_job_->HasEstablishedConnection(),
                final_connect_result == OK);

      EXPECT_THAT(test_delegate_->WaitForResult(),
                  IsError(final_connect_result));
      if (final_connect_result == OK) {
        CheckConnection(kIpV4Endpoint1, service_endpoint2);
      }
      ASSERT_TRUE(client_socket_factory_.AllDataProvidersUsed());
    }
  }
}

// Test the order IPs are connected to when when new, higher priority IPs
// trickle in after connection attempts start.
TEST_F(TcpConnectJobTest, HigherPriorityIpsReceivedLast) {
  // These are in final priority order, but they are received in reverse order
  // of priority.
  ServiceEndpoint service_endpoint1 =
      CreateServiceEndpoint({kIpV4Endpoint1, kIpV4Endpoint2, kIpV6Endpoint1,
                             kIpV6Endpoint2, kIpV6Endpoint3},
                            {"h2"}, /*ech=*/false);
  // Note that these are entirely contained within `service_endpoint1`.
  ServiceEndpoint service_endpoint2 =
      CreateServiceEndpoint({kIpV4Endpoint2, kIpV6Endpoint2}, {"http/1.1"});
  ServiceEndpoint service_endpoint3 = CreateServiceEndpoint(
      {kIpV4Endpoint3, kIpV4Endpoint4, kIpV6Endpoint3, kIpV6Endpoint4});

  for (Error final_connect_result : {OK, ERR_UNEXPECTED}) {
    SCOPED_TRACE(final_connect_result);

    auto request = host_resolver_.AddFakeRequest();
    // AAAA results arrive first.
    request->set_start_callback(base::BindLambdaForTesting([&]() {
      request->set_endpoints({service_endpoint3})
          .CallOnServiceEndpointsUpdated();
    }));

    // Connect to the first IPv6 record from `service_endpoint3`.
    MockConnectCompleter connect_completer1;
    AddConnect(MockConnect(&connect_completer1), kIpV6Endpoint3);
    // Connect to the IPv4 record from `service_endpoint2`, which will be
    // received during the first connection attempt.
    MockConnectCompleter connect_completer2;
    AddConnect(MockConnect(&connect_completer2), kIpV4Endpoint2);
    // Connect order for the records in `service_endpoint1`, starting with the
    // first IPv6 record, and skipping over the last IPv6 record, which was
    // already tried. The SYNC/ASYNC choices are random. Note that
    // kIpV4Endpoint2 and kIpV6Endpoint3 have already been tried.
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV6Endpoint1);
    AddConnect(MockConnect(SYNCHRONOUS, ERR_FAILED), kIpV4Endpoint1);
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV6Endpoint2);
    // Back to the untried entries from `service_endpoint3`, skipping over
    // `service_endpoint2`, as it has no usable records.
    AddConnect(MockConnect(SYNCHRONOUS, ERR_FAILED), kIpV4Endpoint3);
    AddConnect(MockConnect(SYNCHRONOUS, ERR_FAILED), kIpV6Endpoint4);
    AddConnect(MockConnect(ASYNC, final_connect_result), kIpV4Endpoint4);

    EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

    connect_completer1.WaitForConnect();

    // `service_endpoint2` received. They're higher priority than
    // `service_endpoint3`, so preempt them.
    request->set_endpoints({service_endpoint2, service_endpoint3})
        .CallOnServiceEndpointsUpdated();

    connect_completer1.Complete(ERR_FAILED);
    connect_completer2.WaitForConnect();

    // `service_endpoint1` received, preempting the other two groups.
    request
        ->set_endpoints(
            {service_endpoint1, service_endpoint2, service_endpoint3})
        .CallOnServiceEndpointRequestFinished(OK);

    connect_completer2.Complete(ERR_FAILED);

    EXPECT_THAT(test_delegate_->WaitForResult(), IsError(final_connect_result));
    if (final_connect_result == OK) {
      CheckConnection(kIpV4Endpoint4, service_endpoint3);
    }
    ASSERT_TRUE(client_socket_factory_.AllDataProvidersUsed());
  }
}

// Test that ServiceEndpointOverride works as expected.
TEST_F(TcpConnectJobTest, ServiceEndpointOverride) {
  ServiceEndpoint service_endpoint = CreateServiceEndpoint(
      {kIpV4Endpoint1, kIpV4Endpoint2, kIpV6Endpoint1, kIpV6Endpoint2}, {"h2"});
  service_endpoint_override_ =
      TcpConnectJob::ServiceEndpointOverride(service_endpoint, kDnsAliases);

  for (IoMode io_mode : {SYNCHRONOUS, ASYNC}) {
    SCOPED_TRACE(io_mode);
    for (Error final_connect_result : {OK, ERR_UNEXPECTED}) {
      SCOPED_TRACE(final_connect_result);

      AddConnect(MockConnect(io_mode, ERR_FAILED), kIpV6Endpoint1);
      AddConnect(MockConnect(io_mode, ERR_FAILED), kIpV4Endpoint1);
      AddConnect(MockConnect(io_mode, ERR_FAILED), kIpV6Endpoint2);
      AddConnect(MockConnect(io_mode, final_connect_result), kIpV4Endpoint2);

      if (final_connect_result == OK) {
        InitRunAndExpectSuccess(kIpV4Endpoint2, service_endpoint,
                                io_mode == SYNCHRONOUS);
        EXPECT_EQ(kDnsAliases, test_delegate_->socket()->GetDnsAliases());
      } else {
        InitRunAndExpectError(ERR_UNEXPECTED, io_mode == SYNCHRONOUS);
      }
      ASSERT_TRUE(client_socket_factory_.AllDataProvidersUsed());
    }
  }
}

}  // namespace
}  // namespace net
