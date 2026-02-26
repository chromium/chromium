// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_connect_job.h"

#include <array>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
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
#include "net/socket/connection_attempts.h"
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
    // These should have all been consumed.
    EXPECT_TRUE(host_resolution_callback_results_.empty());
    EXPECT_FALSE(last_host_resolution_callback_);
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
        on_host_resolution_callback_, GetSupportedAplns());
  }

  // Initializes `test_delegate_` and `connect_job_`, tearing down old ones
  // first.
  void InitConnectJob() {
    // Destruction order is important here, as the ConnectJob references the
    // delegate.
    connect_job_.reset();
    test_delegate_.reset();

    test_delegate_ = std::make_unique<TestConnectJobDelegate>();
    connect_job_ = std::make_unique<TcpConnectJob>(
        initial_priority_, SocketTag(), &common_connect_job_params_,
        SocketParams(), test_delegate_.get(), /*net_log=*/nullptr,
        service_endpoint_override_);
  }

  // Combines InitConnectJob() and test_delegate_->StartJobExpectingResult(),
  // expecting an error.
  void InitRunAndExpectError(
      Error expected_result,
      bool expect_sync_result,
      const std::vector<ConnectionAttempt>& expected_connection_attempts) {
    DCHECK_NE(expected_result, OK);
    InitConnectJob();
    test_delegate_->StartJobExpectingResult(connect_job_.get(), expected_result,
                                            expect_sync_result);
    EXPECT_EQ(expected_connection_attempts,
              connect_job_->GetConnectionAttempts());
  }

  // Waits for `test_delegate_` to see a failure in an already started
  // TcpConnectJob, performing the same checks on the results as
  // InitRunAndExpectError().
  void WaitForError(
      Error expected_result,
      const std::vector<ConnectionAttempt>& expected_connection_attempts) {
    EXPECT_THAT(test_delegate_->WaitForResult(), IsError(expected_result));
    EXPECT_EQ(expected_connection_attempts,
              connect_job_->GetConnectionAttempts());
  }

  // Combines InitConnectJob() and test_delegate_->StartJobExpectingResult(),
  // expecting success.
  void InitRunAndExpectSuccess(
      const IPEndPoint& expected_ip_endpoint,
      const ServiceEndpoint& expected_service_endpoint,
      bool expect_sync_result,
      const std::vector<ConnectionAttempt>& expected_connection_attempts = {}) {
    InitConnectJob();
    test_delegate_->StartJobExpectingResult(connect_job_.get(), OK,
                                            expect_sync_result);
    CheckConnection(expected_ip_endpoint, expected_service_endpoint);
    EXPECT_EQ(expected_connection_attempts,
              connect_job_->GetConnectionAttempts());
  }

  // Waits for `test_delegate_` to see a success in an already started
  // TcpConnectJob, performing the same checks on the results as
  // InitRunAndExpectSuccess().
  void WaitForSuccess(
      const IPEndPoint& expected_ip_endpoint,
      const ServiceEndpoint& expected_service_endpoint,
      const std::vector<ConnectionAttempt>& expected_connection_attempts = {}) {
    EXPECT_THAT(test_delegate_->WaitForResult(), IsOk());
    CheckConnection(expected_ip_endpoint, expected_service_endpoint);
    EXPECT_THAT(connect_job_->GetConnectionAttempts(),
                testing::ElementsAreArray(expected_connection_attempts));
  }

  // Checks that there's a socket, and it's connected to the specified
  // endpoints.
  void CheckConnection(const IPEndPoint& expected_ip_endpoint,
                       const ServiceEndpoint& expected_service_endpoint) const {
    // This destroys the ServiceEndpoint held by the ConnectJob, but this is the
    // only place it's used in these tests, so going so is fine.
    EXPECT_EQ(connect_job_->PassServiceEndpoint(), expected_service_endpoint);

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

  // When called, ConnectJobs will be created with a non-null
  // OnHostResolutionCallback and `host_resolution_callback_info_` will added to
  // with each callback invocation. `host_resolution_callback_results` must
  // contain the return value for each callback invocation.
  // `last_host_resolution_callback` will be invoked asynchronously just after
  // the last result has been returned, if non-null. The length of
  // `host_resolution_callback_results` must exactly match the number of
  // OnHostResolutionCallback() invocations.
  void EnableHostResolutionCallbacks(
      std::list<OnHostResolutionCallbackResult>
          host_resolution_callback_results,
      base::OnceClosure last_host_resolution_callback = base::OnceClosure()) {
    // All previous OnHostResolutionCallback information should have been
    // consumed.
    DCHECK(host_resolution_callback_results_.empty());
    DCHECK(!last_host_resolution_callback_);

    // Clear any information from previous callbacks.
    host_resolution_callback_info_.clear();

    host_resolution_callback_results_ =
        std::move(host_resolution_callback_results);
    last_host_resolution_callback_ = std::move(last_host_resolution_callback);
    if (last_host_resolution_callback_) {
      CHECK_GT(host_resolution_callback_results_.size(), 0u);
    }
    on_host_resolution_callback_ = base::BindRepeating(
        &TcpConnectJobTest::OnHostResolution, base::Unretained(this));
  }

 protected:
  // Stores the information passed to the OnHostResolutionCallback() for
  // validation.
  struct HostResolutionCallbackInfo {
    std::vector<ServiceEndpoint> service_endpoints;
    std::set<std::string> aliases;

    bool operator==(const HostResolutionCallbackInfo&) const = default;
  };

  // IPs used by tests. Numbers are chosen to make it easy to identify them from
  // failure output.
  const IPEndPoint kIpV4Endpoint1 = MakeIPEndPoint("4.1.1.1", 41);
  const IPEndPoint kIpV4Endpoint2 = MakeIPEndPoint("4.2.2.2", 42);
  const IPEndPoint kIpV4Endpoint3 = MakeIPEndPoint("4.3.3.3", 43);
  const IPEndPoint kIpV4Endpoint4 = MakeIPEndPoint("4.4.4.4", 44);
  const IPEndPoint kIpV6Endpoint1 = MakeIPEndPoint("6::1", 61);
  const IPEndPoint kIpV6Endpoint2 = MakeIPEndPoint("6::2", 62);
  const IPEndPoint kIpV6Endpoint3 = MakeIPEndPoint("6::3", 63);
  const IPEndPoint kIpV6Endpoint4 = MakeIPEndPoint("6::4", 64);

  OnHostResolutionCallbackResult OnHostResolution(
      const HostPortPair& host_port_pair,
      const HostResolverEndpointsOrServiceEndpoints& endpoint_results,
      const std::set<std::string>& aliases) {
    // These are the same for all tests, so can test them here.
    EXPECT_EQ(host_port_pair.host(), kHostName);
    EXPECT_EQ(host_port_pair.port(), 443);

    CHECK(std::holds_alternative<base::span<const ServiceEndpoint>>(
        endpoint_results));
    auto service_endpoints =
        std::get<base::span<const ServiceEndpoint>>(endpoint_results);
    host_resolution_callback_info_.emplace_back(
        base::ToVector(service_endpoints), aliases);

    // Get result to return.
    CHECK(!host_resolution_callback_results_.empty());
    OnHostResolutionCallbackResult result =
        host_resolution_callback_results_.front();
    host_resolution_callback_results_.pop_front();

    // If this was the last call, invoke `last_host_resolution_callback_`
    // asynchronously.
    if (host_resolution_callback_results_.empty() &&
        last_host_resolution_callback_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(last_host_resolution_callback_));
    }

    return result;
  }

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

  // Priority used when making a new ConnectJob.
  RequestPriority initial_priority_ = DEFAULT_PRIORITY;

  // These are all related to testing OnHostResolutionCallback support.
  OnHostResolutionCallback on_host_resolution_callback_;
  // Cached information from all previous `OnHostResolutionCallback` calls.
  std::vector<HostResolutionCallbackInfo> host_resolution_callback_info_;
  // Return values for each OnHostResolutionCallback invocation.
  std::list<OnHostResolutionCallbackResult> host_resolution_callback_results_;
  // Invoked asynchronously after last OnHostResolutionCallback invocation.
  base::OnceClosure last_host_resolution_callback_;
};

// Test that the priority is correctly plumbed down to the
// ServiceEndpointRequest, both for the initial priority, and when modifying the
// priority.
TEST_F(TcpConnectJobTest, Priority) {
  // Not a full list.
  const auto kPriorities =
      std::to_array<RequestPriority>({IDLE, MEDIUM, HIGHEST});

  for (RequestPriority initial_priority : kPriorities) {
    initial_priority_ = initial_priority;

    MockConnectCompleter connect_completer;
    AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);

    // Start request, make sure initial priority is passed along.
    auto request = host_resolver_.AddFakeRequest();
    EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));
    EXPECT_EQ(request->priority(), initial_priority);

    // Change priority before any DNS results have been received.
    for (RequestPriority priority : kPriorities) {
      connect_job_->ChangePriority(priority);
      EXPECT_EQ(request->priority(), priority);
    }

    // Simulate update. It doesn't provide any IPs, so we shouldn't try to
    // connect to anything.
    request->set_crypto_ready(true).CallOnServiceEndpointsUpdated();

    // Changing priority should still work.
    for (RequestPriority priority : kPriorities) {
      connect_job_->ChangePriority(priority);
      EXPECT_EQ(request->priority(), priority);
    }

    // DNS requests completes, and we start to connect.
    request->add_endpoint(CreateServiceEndpoint({kIpV4Endpoint1}))
        .CallOnServiceEndpointRequestFinished(OK);

    // Changing priority should still work not crash, even after the DNS request
    // is complete. While the request still exists here, and its priority is
    // updated, don't check it, since it doesn't matter any more, and we could
    // theoretically change behavior.
    for (RequestPriority priority : kPriorities) {
      connect_job_->ChangePriority(priority);
    }

    // The request fails.
    connect_completer.WaitForConnectAndComplete(ERR_FAILED);
    WaitForError(ERR_FAILED, /*expected_connection_attempts=*/{
                     {kIpV4Endpoint1, ERR_FAILED}});

    // Changing priority should still not crash, even after the DNS request was
    // destroyed. Can't check the request's priority, since it's been destroyed.
    for (RequestPriority priority : kPriorities) {
      connect_job_->ChangePriority(priority);
    }
  }
}

TEST_F(TcpConnectJobTest, DnsErrorSync) {
  // Use something other than ERR_NAME_NOT_RESOLVED because that's the default
  // error in some cases.
  host_resolver_.ConfigureDefaultResolution()
      .CompleteStartSynchronously(ERR_FAILED)
      .set_resolve_error_info(kResolveErrorInfo);
  InitRunAndExpectError(
      ERR_FAILED, /*expect_sync_result=*/true,
      /*expected_connection_attempts=*/{{IPEndPoint(), ERR_FAILED}});
  EXPECT_EQ(connect_job_->GetResolveErrorInfo(), kResolveErrorInfo);
}

TEST_F(TcpConnectJobTest, DnsErrorAsync) {
  // Use something other than ERR_NAME_NOT_RESOLVED because that's the default
  // error in some cases.
  host_resolver_.AddFakeRequest()
      ->set_resolve_error_info(kResolveErrorInfo)
      .CompleteStartAsynchronously(ERR_FAILED);
  InitRunAndExpectError(
      ERR_FAILED, /*expect_sync_result=*/false,
      /*expected_connection_attempts=*/{{IPEndPoint(), ERR_FAILED}});
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
    InitRunAndExpectError(
        ERR_FAILED, /*expect_sync_result=*/false,
        /*expected_connection_attempts=*/{{IPEndPoint(), ERR_FAILED}});
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
    InitRunAndExpectError(
        ERR_FAILED, /*expect_sync_result=*/false,
        /*expected_connection_attempts=*/{{IPEndPoint(), ERR_FAILED}});
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
  InitRunAndExpectError(
      ERR_FAILED, /*expect_sync_result=*/false,
      /*expected_connection_attempts=*/{{IPEndPoint(), ERR_FAILED}});
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  EXPECT_EQ(connect_job_->GetResolveErrorInfo(), kResolveErrorInfo);
}

// Test the case where there's a DNS error after a connection error. The DNS
// error should be the only error in ConnectionAttempts, and should be what the
// ConnectJob fails with.
TEST_F(TcpConnectJobTest, DnsErrorAfterConnectError) {
  MockConnectCompleter connect_completer;
  AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);
  auto request = host_resolver_.AddFakeRequest();
  request->set_start_callback(base::BindLambdaForTesting([&]() {
    request->set_crypto_ready(false)
        .add_endpoint(CreateServiceEndpoint({kIpV4Endpoint1}))
        .CallOnServiceEndpointsUpdated();
    connect_completer.WaitForConnectAndComplete(ERR_UNEXPECTED);
    EXPECT_FALSE(connect_job_->HasEstablishedConnection());
    request->set_resolve_error_info(kResolveErrorInfo)
        .CallOnServiceEndpointRequestFinished(ERR_FAILED);
  }));
  InitRunAndExpectError(
      ERR_FAILED, /*expect_sync_result=*/false,
      /*expected_connection_attempts=*/{{IPEndPoint(), ERR_FAILED}});
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
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
  InitRunAndExpectError(
      ERR_FAILED, /*expect_sync_result=*/true,
      /*expected_connection_attempts=*/{{kIpV4Endpoint1, ERR_FAILED}});
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
  InitRunAndExpectError(
      ERR_FAILED, /*expect_sync_result=*/false,
      /*expected_connection_attempts=*/{{kIpV4Endpoint1, ERR_FAILED}});
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
  InitRunAndExpectError(
      ERR_FAILED, /*expect_sync_result=*/false,
      /*expected_connection_attempts=*/{{kIpV4Endpoint1, ERR_FAILED}});
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
  InitRunAndExpectError(
      ERR_FAILED, /*expect_sync_result=*/false,
      /*expected_connection_attempts=*/{{kIpV4Endpoint1, ERR_FAILED}});
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
                                    /*expect_sync_result=*/everything_sync,
                                    /*expected_connection_attempts=*/
                                    {{kIpV6Endpoint1, ERR_FAILED}});
          } else {
            InitRunAndExpectError(second_connect_result,
                                  /*expect_sync_result=*/everything_sync,
                                  /*expected_connection_attempts=*/
                                  {{kIpV6Endpoint1, ERR_FAILED},
                                   {kIpV6Endpoint2, second_connect_result}});
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
                            /*expect_sync_result=*/everything_sync,
                            /*expected_connection_attempts=*/
                            {{kIpV6Endpoint1, ERR_NETWORK_IO_SUSPENDED}});
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
    // ServiceEndpoint that is expected to be returned by PassServiceEndpoint().
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
                              /*expect_sync_result=*/false,
                              /*expected_connection_attempts=*/
                              {{IPEndPoint(), ERR_NAME_NOT_RESOLVED}});
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
  InitRunAndExpectError(
      ERR_NAME_NOT_RESOLVED, /*expect_sync_result=*/true,
      /*expected_connection_attempts=*/{{IPEndPoint(), ERR_NAME_NOT_RESOLVED}});

  // Ech will still disable non-svcb records.
  host_resolver_.ConfigureDefaultResolution()
      .add_endpoint(
          CreateServiceEndpoint({kIpV4Endpoint1}, {"h2"}, /*ech=*/true))
      .add_endpoint(CreateServiceEndpoint({kIpV6Endpoint1}))
      .CompleteStartSynchronously(OK);
  InitRunAndExpectError(
      ERR_NAME_NOT_RESOLVED, /*expect_sync_result=*/true,
      /*expected_connection_attempts=*/{{IPEndPoint(), ERR_NAME_NOT_RESOLVED}});

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
  InitRunAndExpectError(
      ERR_NAME_NOT_RESOLVED, /*expect_sync_result=*/true,
      /*expected_connection_attempts=*/{{IPEndPoint(), ERR_NAME_NOT_RESOLVED}});

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
    std::vector<ConnectionAttempt> expected_connection_attempts;
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV6Endpoint1);
    expected_connection_attempts.emplace_back(kIpV6Endpoint1, ERR_FAILED);
    AddConnect(MockConnect(ASYNC, ERR_INVALID_ARGUMENT), kIpV4Endpoint1);
    expected_connection_attempts.emplace_back(kIpV4Endpoint1,
                                              ERR_INVALID_ARGUMENT);
    AddConnect(MockConnect(ASYNC, ERR_INVALID_ARGUMENT), kIpV6Endpoint2);
    expected_connection_attempts.emplace_back(kIpV6Endpoint2,
                                              ERR_INVALID_ARGUMENT);
    AddConnect(MockConnect(ASYNC, ERR_ACCESS_DENIED), kIpV4Endpoint2);
    expected_connection_attempts.emplace_back(kIpV4Endpoint2,
                                              ERR_ACCESS_DENIED);
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV6Endpoint3);
    expected_connection_attempts.emplace_back(kIpV6Endpoint3, ERR_FAILED);
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV4Endpoint3);
    expected_connection_attempts.emplace_back(kIpV4Endpoint3, ERR_FAILED);
    AddConnect(MockConnect(ASYNC, final_connect_result), kIpV6Endpoint4);

    if (final_connect_result == OK) {
      InitRunAndExpectSuccess(kIpV6Endpoint4, service_endpoints.back(),
                              /*expect_sync_result=*/false,
                              expected_connection_attempts);
    } else {
      expected_connection_attempts.emplace_back(kIpV6Endpoint4,
                                                final_connect_result);
      InitRunAndExpectError(final_connect_result,
                            /*expect_sync_result=*/false,
                            expected_connection_attempts);
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
    std::vector<ConnectionAttempt> expected_connection_attempts;
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV6Endpoint1);
    expected_connection_attempts.emplace_back(kIpV6Endpoint1, ERR_FAILED);
    AddConnect(MockConnect(ASYNC, ERR_ACCESS_DENIED), kIpV4Endpoint1);
    expected_connection_attempts.emplace_back(kIpV4Endpoint1,
                                              ERR_ACCESS_DENIED);
    AddConnect(MockConnect(ASYNC, ERR_INVALID_ARGUMENT), kIpV6Endpoint2);
    expected_connection_attempts.emplace_back(kIpV6Endpoint2,
                                              ERR_INVALID_ARGUMENT);
    AddConnect(MockConnect(ASYNC, ERR_ACCESS_DENIED), kIpV4Endpoint2);
    expected_connection_attempts.emplace_back(kIpV4Endpoint2,
                                              ERR_ACCESS_DENIED);
    AddConnect(MockConnect(ASYNC, ERR_INVALID_ARGUMENT), kIpV6Endpoint4);
    expected_connection_attempts.emplace_back(kIpV6Endpoint4,
                                              ERR_INVALID_ARGUMENT);
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV4Endpoint4);
    expected_connection_attempts.emplace_back(kIpV4Endpoint4, ERR_FAILED);
    AddConnect(MockConnect(ASYNC, final_connect_result), kIpV4Endpoint3);
    if (final_connect_result == OK) {
      InitRunAndExpectSuccess(kIpV4Endpoint3, service_endpoints.back(),
                              /*expect_sync_result=*/false,
                              expected_connection_attempts);
    } else {
      expected_connection_attempts.emplace_back(kIpV4Endpoint3,
                                                final_connect_result);
      InitRunAndExpectError(final_connect_result,
                            /*expect_sync_result=*/false,
                            expected_connection_attempts);
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
    WaitForError(ERR_NAME_NOT_RESOLVED, /*expected_connection_attempts=*/{
                     {IPEndPoint(), ERR_NAME_NOT_RESOLVED}});
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

    WaitForSuccess(kIpV6Endpoint1, service_endpoint_https);
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

    WaitForSuccess(kIpV6Endpoint1, service_endpoint_https);
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

    WaitForSuccess(kIpV6Endpoint1, service_endpoint_https);
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

      if (final_connect_result == OK) {
        WaitForSuccess(
            kIpV4Endpoint1, service_endpoint2,
            /*expected_connection_attempts=*/{{kIpV6Endpoint1, ERR_FAILED}});
      } else {
        WaitForError(final_connect_result, /*expected_connection_attempts=*/{
                         {kIpV6Endpoint1, ERR_FAILED},
                         {kIpV4Endpoint1, final_connect_result}});
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

    std::vector<ConnectionAttempt> expected_connection_attempts;
    // Connect to the first IPv6 record from `service_endpoint3`.
    MockConnectCompleter connect_completer1;
    AddConnect(MockConnect(&connect_completer1), kIpV6Endpoint3);
    expected_connection_attempts.emplace_back(kIpV6Endpoint3, ERR_FAILED);
    // Connect to the IPv4 record from `service_endpoint2`, which will be
    // received during the first connection attempt.
    MockConnectCompleter connect_completer2;
    AddConnect(MockConnect(&connect_completer2), kIpV4Endpoint2);
    expected_connection_attempts.emplace_back(kIpV4Endpoint2, ERR_FAILED);
    // Connect order for the records in `service_endpoint1`, starting with the
    // first IPv6 record, and skipping over the last IPv6 record, which was
    // already tried. The SYNC/ASYNC choices are random. Note that
    // kIpV4Endpoint2 and kIpV6Endpoint3 have already been tried.
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV6Endpoint1);
    expected_connection_attempts.emplace_back(kIpV6Endpoint1, ERR_FAILED);
    AddConnect(MockConnect(SYNCHRONOUS, ERR_FAILED), kIpV4Endpoint1);
    expected_connection_attempts.emplace_back(kIpV4Endpoint1, ERR_FAILED);
    AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV6Endpoint2);
    expected_connection_attempts.emplace_back(kIpV6Endpoint2, ERR_FAILED);
    // Back to the untried entries from `service_endpoint3`, skipping over
    // `service_endpoint2`, as it has no usable records.
    AddConnect(MockConnect(SYNCHRONOUS, ERR_FAILED), kIpV4Endpoint3);
    expected_connection_attempts.emplace_back(kIpV4Endpoint3, ERR_FAILED);
    AddConnect(MockConnect(SYNCHRONOUS, ERR_FAILED), kIpV6Endpoint4);
    expected_connection_attempts.emplace_back(kIpV6Endpoint4, ERR_FAILED);
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

    if (final_connect_result == OK) {
      WaitForSuccess(kIpV4Endpoint4, service_endpoint3,
                     expected_connection_attempts);
    } else {
      expected_connection_attempts.emplace_back(kIpV4Endpoint4,
                                                final_connect_result);
      WaitForError(final_connect_result, expected_connection_attempts);
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

      std::vector<ConnectionAttempt> expected_connection_attempts;
      AddConnect(MockConnect(io_mode, ERR_FAILED), kIpV6Endpoint1);
      expected_connection_attempts.emplace_back(kIpV6Endpoint1, ERR_FAILED);
      AddConnect(MockConnect(io_mode, ERR_FAILED), kIpV4Endpoint1);
      expected_connection_attempts.emplace_back(kIpV4Endpoint1, ERR_FAILED);
      AddConnect(MockConnect(io_mode, ERR_FAILED), kIpV6Endpoint2);
      expected_connection_attempts.emplace_back(kIpV6Endpoint2, ERR_FAILED);
      AddConnect(MockConnect(io_mode, final_connect_result), kIpV4Endpoint2);

      if (final_connect_result != OK) {
        expected_connection_attempts.emplace_back(kIpV4Endpoint2,
                                                  ERR_UNEXPECTED);
      }

      // Split out sync and async cases, so can set the priority in the async
      // cases, and make sure there's no crash.
      if (io_mode == SYNCHRONOUS) {
        if (final_connect_result == OK) {
          InitRunAndExpectSuccess(kIpV4Endpoint2, service_endpoint,
                                  /*expect_sync_result=*/true,
                                  expected_connection_attempts);
          EXPECT_EQ(kDnsAliases, test_delegate_->socket()->GetDnsAliases());
        } else {
          connect_job_->ChangePriority(HIGHEST);
          InitRunAndExpectError(ERR_UNEXPECTED, /*expect_sync_result=*/true,
                                expected_connection_attempts);
        }
      } else {
        EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));
        // This should not dereference the null ServiceEndpointRequest.
        connect_job_->ChangePriority(HIGHEST);
        if (final_connect_result == OK) {
          WaitForSuccess(kIpV4Endpoint2, service_endpoint,
                         expected_connection_attempts);
          EXPECT_EQ(kDnsAliases, test_delegate_->socket()->GetDnsAliases());
        } else {
          WaitForError(ERR_UNEXPECTED, expected_connection_attempts);
        }
      }
      ASSERT_TRUE(client_socket_factory_.AllDataProvidersUsed());
    }
  }
}

// Check that the DNS request is destroyed on error, and thus can't call back
// into the ConnectJob.
TEST_F(TcpConnectJobTest, RequestDestroyedOnError) {
  auto request = host_resolver_.AddFakeRequest();
  request->add_endpoint(CreateServiceEndpoint({kIpV4Endpoint1}))
      .CompleteStartAsynchronously(OK);
  AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV4Endpoint1);
  InitRunAndExpectError(
      ERR_FAILED, /*expect_sync_result=*/false,
      /*expected_connection_attempts=*/{{kIpV4Endpoint1, ERR_FAILED}});
  EXPECT_FALSE(request);
}

// Check that the DNS request is destroyed on success, and thus can't call back
// into the ConnectJob.
TEST_F(TcpConnectJobTest, RequestDestroyedOnSuccess) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  auto request = host_resolver_.AddFakeRequest();
  request->add_endpoint(service_endpoint).CompleteStartAsynchronously(OK);
  AddConnect(MockConnect(ASYNC, OK), kIpV4Endpoint1);
  InitRunAndExpectSuccess(kIpV4Endpoint1, service_endpoint,
                          /*expect_sync_result=*/false);
  EXPECT_FALSE(request);
}

////////////////////////////////////
// OnHostResolutionCallback tests //
////////////////////////////////////

TEST_F(TcpConnectJobTest, NoOnHostResolutionCallbackOnDnsError) {
  host_resolver_.AddFakeRequest()
      ->set_resolve_error_info(kResolveErrorInfo)
      .CompleteStartAsynchronously(ERR_FAILED);
  EnableHostResolutionCallbacks({});
  InitRunAndExpectError(
      ERR_FAILED, /*expect_sync_result=*/false,
      /*expected_connection_attempts=*/{{IPEndPoint(), ERR_FAILED}});
}

TEST_F(TcpConnectJobTest, OnHostResolutionCallbackContinue) {
  // Since the callback is passed the destination converted to a HostPort, want
  // to test with all types of destinations.
  const std::array<TransportSocketParams::Endpoint, 3> kDestinations = {
      url::SchemeHostPort(url::kHttpsScheme, kHostName, 443),
      url::SchemeHostPort(url::kHttpScheme, kHostName, 443),
      HostPortPair(kHostName, 443),
  };

  for (const auto& destination : kDestinations) {
    destination_ = destination;

    const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
    host_resolver_.AddFakeRequest()
        ->add_endpoint(service_endpoint)
        .set_aliases(kDnsAliases)
        .CompleteStartAsynchronously(OK);
    AddConnect(MockConnect(ASYNC, OK), kIpV4Endpoint1);

    bool callback_run = false;
    EnableHostResolutionCallbacks(
        {OnHostResolutionCallbackResult::kContinue},
        base::BindLambdaForTesting([&]() {
          // The ConnectJob should have continued synchronously, so a task
          // posted immediately from the OnHostResolutionCallback should be able
          // to observe that the socket has already been created / the only data
          // provider is already in use.
          EXPECT_TRUE(client_socket_factory_.AllDataProvidersUsed());
          callback_run = true;
        }));

    InitRunAndExpectSuccess(kIpV4Endpoint1, service_endpoint,
                            /*expect_sync_result=*/false);
    EXPECT_EQ(kDnsAliases, test_delegate_->socket()->GetDnsAliases());
    const std::vector<HostResolutionCallbackInfo> expected_host_resolution_info{
        {{service_endpoint}, kDnsAliases}};
    EXPECT_THAT(host_resolution_callback_info_,
                testing::ElementsAreArray(expected_host_resolution_info));
    EXPECT_TRUE(callback_run);
  }
}

TEST_F(TcpConnectJobTest, OnHostResolutionCallbackMayBeDeletedAsyncButItIsNot) {
  // Since the callback is passed the destination converted to a HostPort, want
  // to test with all types of destinations.
  const std::array<TransportSocketParams::Endpoint, 3> kDestinations = {
      url::SchemeHostPort(url::kHttpsScheme, kHostName, 443),
      url::SchemeHostPort(url::kHttpScheme, kHostName, 443),
      HostPortPair(kHostName, 443),
  };

  for (const auto& destination : kDestinations) {
    destination_ = destination;

    const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
    host_resolver_.AddFakeRequest()
        ->add_endpoint(service_endpoint)
        .set_aliases(kDnsAliases)
        .CompleteStartAsynchronously(OK);
    AddConnect(MockConnect(ASYNC, OK), kIpV4Endpoint1);

    bool callback_run = false;
    EnableHostResolutionCallbacks(
        {OnHostResolutionCallbackResult::kMayBeDeletedAsync},
        base::BindLambdaForTesting([&]() {
          // The ConnectJob will continue after receiving the kMayBeDeletedAsync
          // message, but only after a post task, so a task posted immediately
          // from the OnHostResolutionCallback should be able to observe that a
          // socket has not yet been created. It will be created
          // immediately after this task is run, from the next task.
          EXPECT_FALSE(client_socket_factory_.AllDataProvidersUsed());
          callback_run = true;
        }));

    InitRunAndExpectSuccess(kIpV4Endpoint1, service_endpoint,
                            /*expect_sync_result=*/false);
    EXPECT_EQ(kDnsAliases, test_delegate_->socket()->GetDnsAliases());
    const std::vector<HostResolutionCallbackInfo> expected_host_resolution_info{
        {{service_endpoint}, kDnsAliases}};
    EXPECT_THAT(host_resolution_callback_info_,
                testing::ElementsAreArray(expected_host_resolution_info));
    EXPECT_TRUE(callback_run);
    EXPECT_TRUE(client_socket_factory_.AllDataProvidersUsed());
  }
}

TEST_F(TcpConnectJobTest, OnHostResolutionCallbackMayBeDeletedAsyncAndItIs) {
  // Since the callback is passed the destination converted to a HostPort, want
  // to test with all types of destinations.
  const std::array<TransportSocketParams::Endpoint, 3> kDestinations = {
      url::SchemeHostPort(url::kHttpsScheme, kHostName, 443),
      url::SchemeHostPort(url::kHttpScheme, kHostName, 443),
      HostPortPair(kHostName, 443),
  };

  for (const auto& destination : kDestinations) {
    destination_ = destination;

    const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
    host_resolver_.AddFakeRequest()
        ->add_endpoint(service_endpoint)
        .set_aliases(kDnsAliases)
        .CompleteStartAsynchronously(OK);
    // Note that no mock connect data is added for this test. Therefore, if
    // there's any actual connection attempt, the test will fail.

    // The callback deletes the ConnectJob and TestDelegate, as
    // kMayBeDeletedAsync implies might happen. The callback is called after a
    // PostTask, which mimics actual behavior of the real SpdySessionPool.
    base::RunLoop run_loop;
    EnableHostResolutionCallbacks(
        {OnHostResolutionCallbackResult::kMayBeDeletedAsync},
        base::BindLambdaForTesting([&]() {
          connect_job_.reset();
          test_delegate_.reset();
          run_loop.Quit();
        }));

    EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));
    run_loop.Run();
    const std::vector<HostResolutionCallbackInfo> expected_host_resolution_info{
        {{service_endpoint}, kDnsAliases}};
    EXPECT_THAT(host_resolution_callback_info_,
                testing::ElementsAreArray(expected_host_resolution_info));

    // There should be no pending task that causes a crash.
    FastForwardBy(base::Seconds(10));
  }
}

// This test covers the case where multiple calls return kMayBeDeletedAsync,
// while a Connector is busy doing different things.
TEST_F(TcpConnectJobTest, OnHostResolutionCallbackMultipleMayBeDeletedAsync) {
  // These are received in reverse order.
  const auto service_endpoint1 = CreateServiceEndpoint({kIpV4Endpoint1});
  const auto service_endpoint2 = CreateServiceEndpoint({kIpV4Endpoint2});
  const auto service_endpoint3 = CreateServiceEndpoint({kIpV4Endpoint3});

  auto request = host_resolver_.AddFakeRequest();

  // There are 4 calls - one update per ServiceEndpoint, and then another on
  // completion.
  EnableHostResolutionCallbacks(
      {OnHostResolutionCallbackResult::kMayBeDeletedAsync,
       OnHostResolutionCallbackResult::kMayBeDeletedAsync,
       OnHostResolutionCallbackResult::kMayBeDeletedAsync,
       OnHostResolutionCallbackResult::kMayBeDeletedAsync});

  MockConnectCompleter connect_completer1;
  MockConnectCompleter connect_completer3;
  AddConnect(MockConnect(&connect_completer1), kIpV4Endpoint3);
  AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV4Endpoint2);
  AddConnect(MockConnect(&connect_completer3), kIpV4Endpoint1);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  request->set_endpoints({service_endpoint3}).CallOnServiceEndpointsUpdated();
  connect_completer1.WaitForConnect();

  // Second update happens while still connecting to the first endpoint, so
  // shouldn trigger any new connection attempts.
  request->set_endpoints({service_endpoint2, service_endpoint3})
      .CallOnServiceEndpointsUpdated();
  // Complete all tasks, so make sure the async task triggered by the
  // kMayBeDeletedAsync result has run.
  FastForwardBy(base::Milliseconds(1));

  // First connection attempt fails. This should trigger the second connection
  // attempt, which also fails.
  connect_completer1.Complete(ERR_FAILED);
  // Complete all tasks, to make sure both connection attempts have failed.
  FastForwardBy(base::Milliseconds(1));

  // Third update happens while idle. It's the crypto complete message, and also
  // adds all the aliases, but still waiting on more IPs to connect to, so
  // nothing happens.
  request->set_aliases(kDnsAliases)
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated();
  // Complete all tasks, so make sure the async task triggered by the
  // kMayBeDeletedAsync result has run.
  FastForwardBy(base::Milliseconds(1));

  EXPECT_FALSE(connect_completer3.is_connecting());

  // Last update happens.
  request
      ->set_endpoints({service_endpoint1, service_endpoint2, service_endpoint3})
      .CallOnServiceEndpointRequestFinished(OK);
  // Request completes successfully.
  connect_completer3.WaitForConnectAndComplete(OK);

  WaitForSuccess(kIpV4Endpoint1, service_endpoint1,
                 /*expected_connection_attempts=*/
                 {{kIpV4Endpoint3, ERR_FAILED}, {kIpV4Endpoint2, ERR_FAILED}});

  const std::vector<HostResolutionCallbackInfo> expected_host_resolution_info{
      {{service_endpoint3}, /*dns_aliases=*/{}},
      {{service_endpoint2, service_endpoint3}, /*dns_aliases=*/{}},
      {{service_endpoint2, service_endpoint3}, kDnsAliases},
      {{service_endpoint1, service_endpoint2, service_endpoint3}, kDnsAliases}};
  EXPECT_THAT(host_resolution_callback_info_,
              testing::ElementsAreArray(expected_host_resolution_info));
}

/////////////////////////////////////////////////////////////
// Tests below this point focus on the two-Connector case. //
/////////////////////////////////////////////////////////////

// If the first information that comes from DNS is slow, only one Connector is
// used.
TEST_F(TcpConnectJobTest, OneConnectorSlowDns) {
  const auto service_endpoint =
      CreateServiceEndpoint({kIpV4Endpoint1, kIpV6Endpoint1});

  auto request = host_resolver_.AddFakeRequest();
  MockConnectCompleter connect_completer;
  AddConnect(MockConnect(&connect_completer), kIpV6Endpoint1);
  AddConnect(MockConnect(ASYNC, OK), kIpV4Endpoint1);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));
  FastForwardBy(TcpConnectJob::kIPv6FallbackTime);

  request->add_endpoint(service_endpoint)
      .CallOnServiceEndpointRequestFinished(OK);
  connect_completer.WaitForConnect();

  EXPECT_FALSE(connect_job_->has_two_connectors_for_testing());
  // Since there's only one Connector, the kIpV4Endpoint1 connection should
  // still be pending.
  EXPECT_FALSE(client_socket_factory_.AllDataProvidersUsed());

  // Failing the first request should create a second.
  connect_completer.Complete(ERR_FAILED);
  EXPECT_TRUE(client_socket_factory_.AllDataProvidersUsed());

  WaitForSuccess(
      kIpV4Endpoint1, service_endpoint,
      /*expected_connection_attempts=*/{{kIpV6Endpoint1, ERR_FAILED}});
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
}

// Test the case where we create two Connectors, but one is never used, since
// there's only one IP. There's no extra observable events here due to the
// second connector, but good to make sure this case doesn't have observable
// problems.
TEST_F(TcpConnectJobTest, TwoConnectorsOneIpSuccess) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  host_resolver_.ConfigureDefaultResolution()
      .add_endpoint(service_endpoint)
      .CompleteStartSynchronously(OK);

  MockConnectCompleter connect_completer;
  AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));
  connect_completer.WaitForConnect();
  EXPECT_FALSE(connect_job_->has_two_connectors_for_testing());
  FastForwardBy(TcpConnectJob::kIPv6FallbackTime);
  EXPECT_TRUE(connect_job_->has_two_connectors_for_testing());

  connect_completer.Complete(OK);
  WaitForSuccess(kIpV4Endpoint1, service_endpoint);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
}

// Test the case where we create two Connectors, but one is never used, since
// there's only one IP. In this case, we ultimately fail to establish any
// connection. There's no extra observable events here due to the second
// connector, but good to make sure this case doesn't have observable problems.
TEST_F(TcpConnectJobTest, TwoConnectorsOneIpFailure) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  host_resolver_.ConfigureDefaultResolution()
      .add_endpoint(service_endpoint)
      .CompleteStartSynchronously(OK);

  MockConnectCompleter connect_completer;
  AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));
  connect_completer.WaitForConnect();
  EXPECT_FALSE(connect_job_->has_two_connectors_for_testing());
  FastForwardBy(TcpConnectJob::kIPv6FallbackTime);
  EXPECT_TRUE(connect_job_->has_two_connectors_for_testing());

  connect_completer.Complete(ERR_FAILED);
  WaitForError(ERR_FAILED,
               /*expected_connection_attempts=*/{{kIpV4Endpoint1, ERR_FAILED}});
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
}

// Test the case where we create two Connectors, but one is never used, since
// there's only one remaining IP when it's created. There's no extra observable
// events here due to the second connector, but good to make sure this case
// doesn't have observable problems.
TEST_F(TcpConnectJobTest, TwoConnectorsOneUsedTwoIpsSuccess) {
  const auto service_endpoint =
      CreateServiceEndpoint({kIpV4Endpoint1, kIpV6Endpoint1});
  host_resolver_.ConfigureDefaultResolution()
      .add_endpoint(service_endpoint)
      .CompleteStartSynchronously(OK);

  AddConnect(MockConnect(ASYNC, ERR_FAILED), kIpV6Endpoint1);
  MockConnectCompleter connect_completer;
  AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);

  base::Time start_time = base::Time::Now();
  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));
  connect_completer.WaitForConnect();
  // Check time to make sure that the IPv4 Connector wasn't created.
  EXPECT_EQ(base::Time::Now() - start_time, base::TimeDelta());
  EXPECT_FALSE(connect_job_->has_two_connectors_for_testing());
  FastForwardBy(TcpConnectJob::kIPv6FallbackTime);
  EXPECT_TRUE(connect_job_->has_two_connectors_for_testing());

  connect_completer.Complete(OK);
  WaitForSuccess(
      kIpV4Endpoint1, service_endpoint,
      /*expected_connection_attempts=*/{{kIpV6Endpoint1, ERR_FAILED}});
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
}

// Test the case where we make two Connectors with two IPs. Once succeeds, one
// never completes.
TEST_F(TcpConnectJobTest, TwoConnectorsTwoIpsOneNeverCompletes) {
  const auto service_endpoint =
      CreateServiceEndpoint({kIpV4Endpoint1, kIpV6Endpoint1});

  // 0 indicates the first Connector successfully connects to kIpV6Endpoint1,
  // while 1 indicates the second one successfully connects to kIpV4Endpoint1.
  for (size_t successful_index : {0u, 1u}) {
    base::Time start_time = base::Time::Now();

    host_resolver_.ConfigureDefaultResolution()
        .add_endpoint(service_endpoint)
        .CompleteStartSynchronously(OK);

    std::array<MockConnectCompleter, 2> connect_completers;
    AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
    AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint1);

    EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));
    connect_completers[0].WaitForConnect();
    EXPECT_FALSE(connect_job_->has_two_connectors_for_testing());
    EXPECT_EQ(base::Time::Now() - start_time, base::TimeDelta());

    // Wait for the second Connector to start, which should mean the fallback
    // time has passed.
    connect_completers[1].WaitForConnect();
    EXPECT_TRUE(connect_job_->has_two_connectors_for_testing());
    EXPECT_EQ(base::Time::Now() - start_time, TcpConnectJob::kIPv6FallbackTime);

    connect_completers[successful_index].Complete(OK);

    WaitForSuccess(successful_index == 0 ? kIpV6Endpoint1 : kIpV4Endpoint1,
                   service_endpoint);
  }
}

// Test the case where we make two Connectors with two IPs. Once fails, one
// succeeds.
TEST_F(TcpConnectJobTest, TwoConnectorsTwoIpsOneFails) {
  const auto service_endpoint =
      CreateServiceEndpoint({kIpV4Endpoint1, kIpV6Endpoint1});

  // 0 indicates the first Connector successfully connects to kIpV6Endpoint1,
  // while 1 indicates the second one successfully connects to kIpV4Endpoint1.
  for (size_t successful_index : {0u, 1u}) {
    base::Time start_time = base::Time::Now();

    host_resolver_.ConfigureDefaultResolution()
        .add_endpoint(service_endpoint)
        .CompleteStartSynchronously(OK);

    std::array<MockConnectCompleter, 2> connect_completers;
    AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
    AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint1);

    EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));
    connect_completers[0].WaitForConnect();
    EXPECT_FALSE(connect_job_->has_two_connectors_for_testing());
    EXPECT_EQ(base::Time::Now() - start_time, base::TimeDelta());

    // Wait for the second Connector to start, which should mean the fallback
    // time has passed.
    connect_completers[1].WaitForConnect();
    EXPECT_TRUE(connect_job_->has_two_connectors_for_testing());
    EXPECT_EQ(base::Time::Now() - start_time, TcpConnectJob::kIPv6FallbackTime);

    connect_completers[1 - successful_index].Complete(ERR_FAILED);
    connect_completers[successful_index].Complete(OK);

    WaitForSuccess(successful_index == 0 ? kIpV6Endpoint1 : kIpV4Endpoint1,
                   service_endpoint,
                   /*expected_connection_attempts=*/
                   {{successful_index == 0 ? kIpV4Endpoint1 : kIpV6Endpoint1,
                     ERR_FAILED}});
  }
}

// Test the case where we make two Connectors with two IPs. Both fail with
// different errors.
TEST_F(TcpConnectJobTest, TwoConnectorsTwoIpsBothFail) {
  const std::vector<IPEndPoint> endpoints = {kIpV6Endpoint1, kIpV4Endpoint1};
  const std::vector<Error> errors = {ERR_FAILED, ERR_UNEXPECTED};
  const auto service_endpoint = CreateServiceEndpoint(endpoints);
  // The errors for each endpoint.

  // 0 indicates the first Connector fails first, while 1 indicates the second
  // one successfully does. Failure order should be reflected in the returned
  // error and the order of the connection attempts.
  for (size_t first_failure : {0u, 1u}) {
    int second_failure = 1 - first_failure;
    base::Time start_time = base::Time::Now();

    host_resolver_.ConfigureDefaultResolution()
        .add_endpoint(service_endpoint)
        .CompleteStartSynchronously(OK);

    std::array<MockConnectCompleter, 2> connect_completers;
    AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
    AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint1);

    EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));
    connect_completers[0].WaitForConnect();
    EXPECT_FALSE(connect_job_->has_two_connectors_for_testing());
    EXPECT_EQ(base::Time::Now() - start_time, base::TimeDelta());

    // Wait for the second Connector to start, which should mean the fallback
    // time has passed.
    connect_completers[1].WaitForConnect();
    EXPECT_TRUE(connect_job_->has_two_connectors_for_testing());
    EXPECT_EQ(base::Time::Now() - start_time, TcpConnectJob::kIPv6FallbackTime);

    connect_completers[first_failure].Complete(errors[first_failure]);
    connect_completers[second_failure].Complete(errors[second_failure]);

    WaitForError(errors[second_failure],
                 /*expected_connection_attempts=*/{
                     {endpoints[first_failure], errors[first_failure]},
                     {endpoints[second_failure], errors[second_failure]}});
  }
}

// Test the case where there is basically always both an IPv4 and IPv6 IP
// available, and that each Connector prefers one or the other. There's only a
// single ServiceEndpoint in this test.
TEST_F(TcpConnectJobTest, TwoConnectorsSixIps) {
  const auto service_endpoint =
      CreateServiceEndpoint({kIpV4Endpoint1, kIpV4Endpoint2, kIpV4Endpoint3,
                             kIpV6Endpoint1, kIpV6Endpoint2, kIpV6Endpoint3});

  base::Time start_time = base::Time::Now();

  host_resolver_.ConfigureDefaultResolution()
      .add_endpoint(service_endpoint)
      .CompleteStartSynchronously(OK);

  std::array<MockConnectCompleter, 6> connect_completers;
  // Note that this order is based on the order in which of the previous
  // connection attempts fails first.
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint1);
  AddConnect(MockConnect(&connect_completers[2]), kIpV4Endpoint2);
  AddConnect(MockConnect(&connect_completers[3]), kIpV6Endpoint2);
  AddConnect(MockConnect(&connect_completers[4]), kIpV6Endpoint3);
  AddConnect(MockConnect(&connect_completers[5]), kIpV4Endpoint3);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  connect_completers[0].WaitForConnect();
  EXPECT_FALSE(connect_job_->has_two_connectors_for_testing());
  EXPECT_EQ(base::Time::Now() - start_time, base::TimeDelta());

  // Wait for the second Connector to start, which should mean the fallback time
  // has passed.
  connect_completers[1].WaitForConnect();
  EXPECT_TRUE(connect_job_->has_two_connectors_for_testing());
  EXPECT_EQ(base::Time::Now() - start_time, TcpConnectJob::kIPv6FallbackTime);

  // kIpV4Endpoint1 fails. The IPv4 Connector should try kIpV4Endpoint2.
  connect_completers[1].Complete(ERR_FAILED);
  connect_completers[2].WaitForConnect();

  // kIpV6Endpoint1 fails. The primary Connector should try kIpV6Endpoint2, and
  // after that fails, kIpV6Endpoint3.
  connect_completers[0].Complete(ERR_UNEXPECTED);
  connect_completers[3].WaitForConnectAndComplete(ERR_UNEXPECTED);
  connect_completers[4].WaitForConnect();

  // kIpV4Endpoint2 fails. The IPv4 Connector should try kIpV4Endpoint3, which
  // also fails.
  connect_completers[2].Complete(ERR_UNEXPECTED);
  connect_completers[5].WaitForConnectAndComplete(ERR_FAILED);

  // kIpV6Endpoint3 succeeds, completing the request.
  connect_completers[4].Complete(OK);

  WaitForSuccess(kIpV6Endpoint3, service_endpoint,
                 /*expected_connection_attempts=*/
                 {{kIpV4Endpoint1, ERR_FAILED},
                  {kIpV6Endpoint1, ERR_UNEXPECTED},
                  {kIpV6Endpoint2, ERR_UNEXPECTED},
                  {kIpV4Endpoint2, ERR_UNEXPECTED},
                  {kIpV4Endpoint3, ERR_FAILED}});
  // No more time should have passed since the slow job was started, since time
  // wasn't simulated advancing, and there should have been no other timed delay
  // by TcpConnectJob.
  EXPECT_EQ(base::Time::Now() - start_time, TcpConnectJob::kIPv6FallbackTime);
}

// Test the case where there are two Connectors, and all the IPv6 IPs come in
// and fail and only then do the IPv4 ones come in. both Connectors should try
// both IPv4 and IPv6 IPs. There's only a single ServiceEndpoint in this test,
// though it's updated half-way through.
TEST_F(TcpConnectJobTest, TwoConnectorsIPv6ThenIpv4) {
  const auto service_endpoint = CreateServiceEndpoint(
      {kIpV6Endpoint1, kIpV6Endpoint2, kIpV6Endpoint3, kIpV6Endpoint4,
       kIpV4Endpoint1, kIpV4Endpoint2, kIpV4Endpoint3, kIpV4Endpoint4});

  base::Time start_time = base::Time::Now();

  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 8> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV6Endpoint2);
  AddConnect(MockConnect(&connect_completers[2]), kIpV6Endpoint3);
  AddConnect(MockConnect(&connect_completers[3]), kIpV6Endpoint4);
  AddConnect(MockConnect(&connect_completers[4]), kIpV4Endpoint1);
  AddConnect(MockConnect(&connect_completers[5]), kIpV4Endpoint2);
  AddConnect(MockConnect(&connect_completers[6]), kIpV4Endpoint3);
  AddConnect(MockConnect(&connect_completers[7]), kIpV4Endpoint4);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));
  // Temporary endpoint that only includes AAAA results.
  request
      ->add_endpoint(CreateServiceEndpoint(
          {kIpV6Endpoint1, kIpV6Endpoint2, kIpV6Endpoint3, kIpV6Endpoint4}))
      .CallOnServiceEndpointsUpdated();

  connect_completers[0].WaitForConnect();
  EXPECT_FALSE(connect_job_->has_two_connectors_for_testing());
  EXPECT_EQ(base::Time::Now() - start_time, base::TimeDelta());

  // Wait for the second Connector to start, which should mean the fallback time
  // has passed.
  connect_completers[1].WaitForConnect();
  EXPECT_TRUE(connect_job_->has_two_connectors_for_testing());
  EXPECT_EQ(base::Time::Now() - start_time, TcpConnectJob::kIPv6FallbackTime);

  // kIpV6Endpoint2 fails. The IPv4 Connector should try kIpV6Endpoint3.
  connect_completers[1].Complete(ERR_FAILED);
  connect_completers[2].WaitForConnect();

  // kIpV6Endpoint1 fails. The primary Connector should try kIpV6Endpoint4,
  // which also fails.
  connect_completers[0].Complete(ERR_FAILED);
  connect_completers[3].WaitForConnectAndComplete(ERR_UNEXPECTED);

  // kIpV6Endpoint3 fails.
  connect_completers[2].Complete(ERR_UNEXPECTED);

  // IPv4 IPs come in, and DNS completes.
  request->set_endpoints({service_endpoint})
      .CallOnServiceEndpointRequestFinished(OK);

  // Both connectors now try IPv4 IPs. Use a similar completion pattern as
  // before, though flipping the order so the first one fails first, and failing
  // with ERR_UNEXPECTED before ERR_FAILED. Again, each Connector should try two
  // IPs. One IP succeeds, this time.
  connect_completers[4].WaitForConnect();
  connect_completers[5].WaitForConnect();

  connect_completers[4].Complete(ERR_UNEXPECTED);
  connect_completers[6].WaitForConnect();

  connect_completers[5].Complete(ERR_UNEXPECTED);
  connect_completers[7].WaitForConnectAndComplete(ERR_FAILED);

  connect_completers[6].Complete(OK);

  WaitForSuccess(kIpV4Endpoint3, service_endpoint,
                 /*expected_connection_attempts=*/
                 {{kIpV6Endpoint2, ERR_FAILED},
                  {kIpV6Endpoint1, ERR_FAILED},
                  {kIpV6Endpoint4, ERR_UNEXPECTED},
                  {kIpV6Endpoint3, ERR_UNEXPECTED},
                  {kIpV4Endpoint1, ERR_UNEXPECTED},
                  {kIpV4Endpoint2, ERR_UNEXPECTED},
                  {kIpV4Endpoint4, ERR_FAILED}});
  // No more time should have passed since the slow job was started, since time
  // wasn't simulated advancing, and there should have been no other timed delay
  // by TcpConnectJob.
  EXPECT_EQ(base::Time::Now() - start_time, TcpConnectJob::kIPv6FallbackTime);
}

// Test the case where there are two Connectors with multiple service endpoints,
// all received at once. Each ServiceEndpoint should only be tried after all IPs
// from the previous endpoint have failed. In this test, the primary job only
// tries IPv6 IPs and the IPv4 job only tries IPv4 jobs, just to keep things
// simple.
TEST_F(TcpConnectJobTest, TwoConnectorsMultipleServiceEndpoints) {
  const auto service_endpoint1 =
      CreateServiceEndpoint({kIpV6Endpoint1, kIpV6Endpoint2, kIpV4Endpoint1});
  // This shared kIpV6Endpoint2 with `service_endpoint1`, but it should not be
  // retried.
  const auto service_endpoint2 = CreateServiceEndpoint(
      {kIpV6Endpoint2, kIpV6Endpoint3, kIpV4Endpoint2, kIpV4Endpoint3});
  // This shared kIpV6Endpoint2 and kIpV4Endpoint2 with earlier
  // ServiceEndpoints, but neither should be retried.
  const auto service_endpoint3 = CreateServiceEndpoint(
      {kIpV6Endpoint2, kIpV6Endpoint4, kIpV4Endpoint2, kIpV4Endpoint4});

  base::Time start_time = base::Time::Now();

  host_resolver_.ConfigureDefaultResolution()
      .set_endpoints({service_endpoint1, service_endpoint2, service_endpoint3})
      .CompleteStartSynchronously(OK);

  std::array<MockConnectCompleter, 8> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint1);
  AddConnect(MockConnect(&connect_completers[2]), kIpV6Endpoint2);
  AddConnect(MockConnect(&connect_completers[3]), kIpV4Endpoint2);
  AddConnect(MockConnect(&connect_completers[4]), kIpV6Endpoint3);
  AddConnect(MockConnect(&connect_completers[5]), kIpV4Endpoint3);
  AddConnect(MockConnect(&connect_completers[6]), kIpV6Endpoint4);
  AddConnect(MockConnect(&connect_completers[7]), kIpV4Endpoint4);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  connect_completers[0].WaitForConnect();
  EXPECT_FALSE(connect_job_->has_two_connectors_for_testing());
  EXPECT_EQ(base::Time::Now() - start_time, base::TimeDelta());

  // Wait for the second Connector to start, which should mean the fallback time
  // has passed.
  connect_completers[1].WaitForConnect();
  EXPECT_TRUE(connect_job_->has_two_connectors_for_testing());
  EXPECT_EQ(base::Time::Now() - start_time, TcpConnectJob::kIPv6FallbackTime);

  // kIpV6Endpoint1 and kIpV6Endpoint2 fail. The primary Connector should sit
  // idle, waiting for the last IP from `service_endpoint1` to complete.
  connect_completers[0].Complete(ERR_FAILED);
  connect_completers[2].WaitForConnectAndComplete(ERR_FAILED);
  // Spin message loop, to run any pending task(s).
  FastForwardBy(base::Seconds(1));
  // There should be no pending connection attempt to the next two IPs.
  EXPECT_FALSE(connect_completers[3].is_connecting());
  EXPECT_FALSE(connect_completers[4].is_connecting());

  // Fail the final IP in `service_endpoint1`. This should cause us to start on
  // `service_endpoint2`.
  connect_completers[1].Complete(ERR_UNEXPECTED);

  // The IPv4 job gets next IP, first, since it had the last failure, and the
  // task to wake up the other Connector is posted asynchronously.
  connect_completers[3].WaitForConnect();
  EXPECT_FALSE(connect_completers[4].is_connecting());
  connect_completers[4].WaitForConnect();

  // kIpV4Endpoint2 and kIpV4Endpoint3 fail. Connecting to the final two IPs
  // from `service_endpoint3` should be blocked by the connection attempt to
  // kIpV6Endpoint3, by the primary Connector.
  connect_completers[3].Complete(ERR_FAILED);
  connect_completers[5].WaitForConnectAndComplete(ERR_FAILED);
  // Spin message loop, to run any pending task(s).
  FastForwardBy(base::Seconds(1));
  // There should be no pending connection attempt to the next two IPs.
  EXPECT_FALSE(connect_completers[6].is_connecting());
  EXPECT_FALSE(connect_completers[7].is_connecting());

  // Fail the final IP in `service_endpoint2`. This should cause us to start on
  // `service_endpoint3`.
  connect_completers[4].Complete(ERR_UNEXPECTED);

  // The primary job gets next IP, first, since it had the last failure, and the
  // task to wake up the other Connector is posted asynchronously.
  connect_completers[6].WaitForConnect();
  EXPECT_FALSE(connect_completers[7].is_connecting());
  connect_completers[7].WaitForConnect();

  // Complete the last two IPs. The connection attempt to kIpV4Endpoint4
  // succeeds.
  connect_completers[6].Complete(ERR_FAILED);
  connect_completers[7].Complete(OK);

  WaitForSuccess(kIpV4Endpoint4, service_endpoint3,
                 /*expected_connection_attempts=*/
                 {{kIpV6Endpoint1, ERR_FAILED},
                  {kIpV6Endpoint2, ERR_FAILED},
                  {kIpV4Endpoint1, ERR_UNEXPECTED},
                  {kIpV4Endpoint2, ERR_FAILED},
                  {kIpV4Endpoint3, ERR_FAILED},
                  {kIpV6Endpoint3, ERR_UNEXPECTED},
                  {kIpV6Endpoint4, ERR_FAILED}});
}

// Test the with two Connectors where the endpoint index goes backwards. This is
// a pretty unusual situation, since generally A/AAAA will complete first, and
// the maximum index will be 0, until the HTTPS record completes, but that could
// change in the future, and it should work.
TEST_F(TcpConnectJobTest, TwoConnectorsEndpointIndexBackwards) {
  // This test will start with only endpoints 3 and 4. Then, after the IP in 3
  // fails, and we're connecting to the first two endpoints in
  // `service_endpoint4`, the DNS resolution completes, proving all endpoints.
  // Then as soon as either of the endpoints in 4 fails, we start with the
  // endpoints in `service_endpoint1`, and then work our way back down to 4
  // again (which would have been the second in the ServiceEndpoints list,
  // initially).
  //
  // This test also covers the case where `primary_connector` is connecting to
  // an IPv4 IP when we make the second connector, so it should become the IPv4
  // Connector when we make a second one.
  const auto service_endpoint1 = CreateServiceEndpoint(
      {kIpV6Endpoint1, kIpV4Endpoint1, kIpV6Endpoint3, kIpV4Endpoint4});
  const auto service_endpoint2 =
      CreateServiceEndpoint({kIpV6Endpoint2, kIpV4Endpoint2, kIpV4Endpoint3});
  const auto service_endpoint3 = CreateServiceEndpoint({kIpV6Endpoint3});
  const auto service_endpoint4 =
      CreateServiceEndpoint({kIpV4Endpoint3, kIpV6Endpoint4, kIpV4Endpoint4,
                             kIpV6Endpoint1, kIpV4Endpoint2});

  base::Time start_time = base::Time::Now();

  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 8> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint3);
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint3);
  AddConnect(MockConnect(&connect_completers[2]), kIpV6Endpoint4);
  AddConnect(MockConnect(&connect_completers[3]), kIpV4Endpoint1);
  AddConnect(MockConnect(&connect_completers[4]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[5]), kIpV4Endpoint4);
  AddConnect(MockConnect(&connect_completers[6]), kIpV4Endpoint2);
  AddConnect(MockConnect(&connect_completers[7]), kIpV6Endpoint2);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // Crypto ready shouldn't actually matter here, but set it, just to make sure
  // it does not.
  request->set_crypto_ready(true)
      .set_endpoints({service_endpoint3, service_endpoint4})
      .CallOnServiceEndpointsUpdated();

  // Fail the only IP in `service_endpoint3` (kIpV6Endpoint3) and move on to the
  // first IPv4 IP in `service_endpoint4`.
  connect_completers[0].WaitForConnectAndComplete(ERR_FAILED);
  connect_completers[1].WaitForConnect();
  EXPECT_FALSE(connect_job_->has_two_connectors_for_testing());
  EXPECT_EQ(base::Time::Now() - start_time, base::TimeDelta());

  // Wait for the second Connector to start, which should mean the fallback time
  // has passed.
  connect_completers[2].WaitForConnect();
  EXPECT_TRUE(connect_job_->has_two_connectors_for_testing());
  EXPECT_EQ(base::Time::Now() - start_time, TcpConnectJob::kIPv6FallbackTime);

  // DNS request completes, with two more ServiceEndpoints.
  request
      ->set_endpoints({service_endpoint1, service_endpoint2, service_endpoint3,
                       service_endpoint4})
      .CallOnServiceEndpointRequestFinished(OK);

  // kIpV4Endpoint3 fails. The IPv4 job should attempt to connect to
  // kIpV4Endpoint1, from `service_endpoint1`.
  connect_completers[1].Complete(ERR_FAILED);
  connect_completers[3].WaitForConnect();

  // kIpV6Endpoint4 fails, the primary job should attempt to connect to
  // kIpV6Endpoint1, which also fails, and then to connect to kIpV4Endpoint4,
  // since kIpV6Endpoint3 has already been tried. That also fails.
  connect_completers[2].Complete(ERR_FAILED);
  connect_completers[4].WaitForConnectAndComplete(ERR_FAILED);
  connect_completers[5].WaitForConnectAndComplete(ERR_FAILED);

  // Spin message loop, to run any pending task(s). There should be no new
  // connection attempt, yet, since we're still working on `service_endpoint1`.
  FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(connect_completers[6].is_connecting());

  // kIpV4Endpoint1 fails, which is the last IP in `service_endpoint1`.
  connect_completers[3].Complete(ERR_FAILED);

  // We try to connect to last two IPs. IPv4 one is first, since it's the IPv4
  // job that had the last failed connection attempt, but that isn't too
  // important.
  connect_completers[6].WaitForConnect();
  connect_completers[7].WaitForConnect();

  // Fail both of those. There should be no more attempts, since every IP has
  // been tried.
  connect_completers[6].Complete(ERR_FAILED);
  connect_completers[7].Complete(ERR_FAILED);

  WaitForError(ERR_FAILED,
               /*expected_connection_attempts=*/
               {{kIpV6Endpoint3, ERR_FAILED},
                {kIpV4Endpoint3, ERR_FAILED},
                {kIpV6Endpoint4, ERR_FAILED},
                {kIpV6Endpoint1, ERR_FAILED},
                {kIpV4Endpoint4, ERR_FAILED},
                {kIpV4Endpoint1, ERR_FAILED},
                {kIpV4Endpoint2, ERR_FAILED},
                {kIpV6Endpoint2, ERR_FAILED}});
}

////////////////////////
// GetLoadState tests //
////////////////////////

TEST_F(TcpConnectJobTest, OneConnectorGetLoadState) {
  const auto service_endpoint1 =
      CreateServiceEndpoint({kIpV6Endpoint2}, {"h2"}, /*ech=*/true);
  const auto service_endpoint2 =
      CreateServiceEndpoint({kIpV4Endpoint1, kIpV6Endpoint1});
  auto request = host_resolver_.AddFakeRequest();

  // There are three attempts. The first connection attempt fails, the second
  // pauses waiting for crypto ready, which disallows the address. The third
  // succeeds.
  std::array<MockConnectCompleter, 3> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint1);
  AddConnect(MockConnect(&connect_completers[2]), kIpV6Endpoint2);

  // Start the request, initially waiting on DNS.
  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));
  std::vector<ConnectionAttempt> expected_connection_attempts;
  EXPECT_EQ(connect_job_->GetLoadState(), LOAD_STATE_RESOLVING_HOST);

  // DNS request returns some IP addresses, but does not complete. We start
  // connecting to the IPv6 endpoint.
  request->add_endpoint(service_endpoint2).CallOnServiceEndpointsUpdated();
  EXPECT_EQ(connect_job_->GetLoadState(), LOAD_STATE_CONNECTING);

  // First IP fails to connect. We wait on the next one.
  connect_completers[0].WaitForConnectAndComplete(ERR_FAILED);
  EXPECT_EQ(connect_job_->GetLoadState(), LOAD_STATE_CONNECTING);

  // Second IP connects successfully, but now we're back to waiting on the DNS
  // request to reach crypto ready.
  connect_completers[1].WaitForConnectAndComplete(OK);
  EXPECT_EQ(connect_job_->GetLoadState(), LOAD_STATE_RESOLVING_HOST);

  // The DNS request completes, returning a new endpoint, and we learn the old
  // endpoints are unusable.
  request->set_endpoints({service_endpoint2, service_endpoint1})
      .CallOnServiceEndpointRequestFinished(OK);
  // We're back to connecting again.
  EXPECT_EQ(connect_job_->GetLoadState(), LOAD_STATE_CONNECTING);

  // Complete the final connection attempt, which should complete the request
  // successfully.
  connect_completers[2].WaitForConnectAndComplete(OK);

  // There no failure record for kIpV6Endpoint1, since it actually succeeded, we
  // just rejected the IP afterwards.
  WaitForSuccess(
      kIpV6Endpoint2, service_endpoint1,
      /*expected_connection_attempts=*/{{kIpV6Endpoint1, ERR_FAILED}});
}

TEST_F(TcpConnectJobTest, TwoConnectorsGetLoadState) {
  const auto service_endpoint =
      CreateServiceEndpoint({kIpV4Endpoint1, kIpV6Endpoint1});
  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 3> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint1);

  // Start the request, initially waiting on DNS.
  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));
  std::vector<ConnectionAttempt> expected_connection_attempts;
  EXPECT_EQ(connect_job_->GetLoadState(), LOAD_STATE_RESOLVING_HOST);

  // DNS request returns some IP addresses, but does not complete. We start
  // connecting to the IPv6 endpoint.
  request->add_endpoint(service_endpoint).CallOnServiceEndpointsUpdated();
  EXPECT_EQ(connect_job_->GetLoadState(), LOAD_STATE_CONNECTING);

  // Connection succeeds, but still need to wait for crypto ready, which is part
  // of the DNS request.
  connect_completers[0].WaitForConnectAndComplete(OK);
  EXPECT_EQ(connect_job_->GetLoadState(), LOAD_STATE_RESOLVING_HOST);

  // Waiting takes a while, so we create a second connector, and return to
  // connecting load state.
  connect_completers[1].WaitForConnect();
  EXPECT_TRUE(connect_job_->has_two_connectors_for_testing());
  EXPECT_EQ(connect_job_->GetLoadState(), LOAD_STATE_CONNECTING);

  // Second connection attempt also succeeds, and also must wait on crypto
  // ready.
  connect_completers[1].Complete(OK);
  EXPECT_EQ(connect_job_->GetLoadState(), LOAD_STATE_RESOLVING_HOST);

  // The DNS request completes. We can now return the primary connector's
  // socket.
  request->CallOnServiceEndpointRequestFinished(OK);
  WaitForSuccess(kIpV6Endpoint1, service_endpoint);
}

}  // namespace
}  // namespace net
