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
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "net/base/address_list.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_handle.h"
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

class TcpConnectJobTestBase {
 public:
  explicit TcpConnectJobTestBase(
      const std::vector<base::test::FeatureRefAndParams>& enabled_features =
          {{features::kHappyEyeballsV2, {}}},
      const std::vector<base::test::FeatureRef>& disabled_features = {}) {
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class TcpConnectJobTest : public TcpConnectJobTestBase,
                          public TestWithTaskEnvironment {
 public:
  explicit TcpConnectJobTest(
      const std::vector<base::test::FeatureRefAndParams>& enabled_features =
          {{features::kHappyEyeballsV2, {}}},
      const std::vector<base::test::FeatureRef>& disabled_features = {})
      : TcpConnectJobTestBase(enabled_features, disabled_features),
        TestWithTaskEnvironment(
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
            &http_server_properties_,
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
        handles::kInvalidNetworkHandle, on_host_resolution_callback_,
        GetSupportedAplns());
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
        service_endpoint_override_, disable_stale_dns_);
    start_time_ = base::TimeTicks::Now();
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

  // Checks ConnectTiming. Note that `dns_end` and `connect_start` should always
  // be the same, so only takes one of them. Also, always uses TimeTicks::Now()
  // for `connect_end`, since it should always be the time the completion
  // callback is invoked, which is typically when tests end.
  void CheckConnectTiming(base::TimeTicks dns_start, base::TimeTicks dns_end) {
    EXPECT_EQ(connect_job_->connect_timing().domain_lookup_start, dns_start);
    EXPECT_EQ(connect_job_->connect_timing().domain_lookup_end, dns_end);
    EXPECT_EQ(connect_job_->connect_timing().connect_start, dns_end);
    EXPECT_EQ(connect_job_->connect_timing().connect_end,
              base::TimeTicks::Now());
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
  bool disable_stale_dns_ = false;

  // Use pointers so can easily re-initialize these.
  std::unique_ptr<TestConnectJobDelegate> test_delegate_;
  HttpServerProperties http_server_properties_;
  std::unique_ptr<TcpConnectJob> connect_job_;

  // Time `connect_job_` was started. Set by InitConnectJob(), rather than on
  // start, since there are a lot of different functions to start it.
  base::TimeTicks start_time_;

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

    auto request = host_resolver_.AddFakeRequest();
    MockConnectCompleter connect_completer;
    AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);

    // Start request, make sure initial priority is passed along.
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
  CheckConnectTiming(/*dns_start=*/start_time_,
                     /*dns_end=*/start_time_);
}

TEST_F(TcpConnectJobTest, DnsErrorAsync) {
  auto request = host_resolver_.AddFakeRequest();
  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // Pass some time, to better check connect timing. Time is short to avoid
  // creating a second Connector.
  FastForwardBy(base::Milliseconds(7));

  // Use something other than ERR_NAME_NOT_RESOLVED because that's the default
  // error in some cases.
  request->set_resolve_error_info(kResolveErrorInfo)
      .CallOnServiceEndpointRequestFinished(ERR_FAILED);
  WaitForError(ERR_FAILED,
               /*expected_connection_attempts=*/{{IPEndPoint(), ERR_FAILED}});
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
  EXPECT_EQ(connect_job_->GetResolveErrorInfo(), kResolveErrorInfo);
  CheckConnectTiming(/*dns_start=*/start_time_,
                     /*dns_end=*/base::TimeTicks::Now());
}

// Test the case where there's a DNS error after the connection attempt starts.
TEST_F(TcpConnectJobTest, DnsErrorAfterConnectStart) {
  for (auto crypto_ready : {false, true}) {
    SCOPED_TRACE(crypto_ready);

    auto request = host_resolver_.AddFakeRequest();
    MockConnectCompleter connect_completer;
    AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);
    EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

    FastForwardBy(base::Milliseconds(5));
    request->set_crypto_ready(crypto_ready)
        .add_endpoint(CreateServiceEndpoint({kIpV4Endpoint1}))
        .CallOnServiceEndpointsUpdated();

    connect_completer.WaitForConnect();
    EXPECT_FALSE(connect_job_->HasEstablishedConnection());

    FastForwardBy(base::Milliseconds(6));
    request->set_resolve_error_info(kResolveErrorInfo)
        .CallOnServiceEndpointRequestFinished(ERR_FAILED);

    WaitForError(ERR_FAILED,
                 /*expected_connection_attempts=*/{{IPEndPoint(), ERR_FAILED}});
    EXPECT_FALSE(connect_job_->HasEstablishedConnection());
    EXPECT_EQ(connect_job_->GetResolveErrorInfo(), kResolveErrorInfo);
    // On DNS error, DNS time covers the entire interval, even if there was a
    // connection attempt earlier.
    CheckConnectTiming(/*dns_start=*/start_time_,
                       /*dns_end=*/base::TimeTicks::Now());
  }
}

// Test the case where there's a DNS error after a connection is established.
// This fails the request if crypto ready has not been received yet.
TEST_F(TcpConnectJobTest, DnsErrorAfterConnectComplete) {
  auto request = host_resolver_.AddFakeRequest();
  MockConnectCompleter connect_completer;
  AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);
  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  FastForwardBy(base::Milliseconds(5));
  request->add_endpoint(CreateServiceEndpoint({kIpV4Endpoint1}))
      .CallOnServiceEndpointsUpdated();

  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
  connect_completer.WaitForConnectAndComplete(OK);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());

  FastForwardBy(base::Milliseconds(6));
  request->set_resolve_error_info(kResolveErrorInfo)
      .CallOnServiceEndpointRequestFinished(ERR_FAILED);

  WaitForError(ERR_FAILED,
               /*expected_connection_attempts=*/{{IPEndPoint(), ERR_FAILED}});
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  EXPECT_EQ(connect_job_->GetResolveErrorInfo(), kResolveErrorInfo);
  // On DNS error, DNS time covers the entire interval, even if there was a
  // connection attempt earlier.
  CheckConnectTiming(/*dns_start=*/start_time_,
                     /*dns_end=*/base::TimeTicks::Now());
}

// Test the case where there's a DNS error after a connection error. The DNS
// error should be the only error in ConnectionAttempts, and should be what the
// ConnectJob fails with.
TEST_F(TcpConnectJobTest, DnsErrorAfterConnectError) {
  auto request = host_resolver_.AddFakeRequest();
  MockConnectCompleter connect_completer;
  AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);
  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  FastForwardBy(base::Milliseconds(5));
  request->add_endpoint(CreateServiceEndpoint({kIpV4Endpoint1}))
      .CallOnServiceEndpointsUpdated();

  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
  connect_completer.WaitForConnectAndComplete(ERR_UNEXPECTED);
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());

  FastForwardBy(base::Milliseconds(6));
  request->set_resolve_error_info(kResolveErrorInfo)
      .CallOnServiceEndpointRequestFinished(ERR_FAILED);

  WaitForError(ERR_FAILED,
               /*expected_connection_attempts=*/{{IPEndPoint(), ERR_FAILED}});
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
  EXPECT_EQ(connect_job_->GetResolveErrorInfo(), kResolveErrorInfo);
  // On DNS error, DNS time covers the entire interval, even if there was a
  // connection attempt earlier.
  CheckConnectTiming(/*dns_start=*/start_time_,
                     /*dns_end=*/base::TimeTicks::Now());
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
  CheckConnectTiming(/*dns_start=*/start_time_,
                     /*dns_end=*/start_time_);
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
  CheckConnectTiming(/*dns_start=*/start_time_,
                     /*dns_end=*/start_time_);
}

TEST_F(TcpConnectJobTest, ConnectionSuccessAsyncDnsSyncConnect) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  auto request = host_resolver_.AddFakeRequest();
  AddConnect(MockConnect(SYNCHRONOUS, OK), kIpV4Endpoint1);
  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  FastForwardBy(base::Milliseconds(5));
  request->add_endpoint(service_endpoint)
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointRequestFinished(OK);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());

  WaitForSuccess(kIpV4Endpoint1, service_endpoint);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  EXPECT_EQ(kDnsAliases, test_delegate_->socket()->GetDnsAliases());
  CheckConnectTiming(/*dns_start=*/start_time_,
                     /*dns_end=*/base::TimeTicks::Now());
}

TEST_F(TcpConnectJobTest, ConnectionErrorAsyncDnsSyncConnect) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  auto request = host_resolver_.AddFakeRequest();
  AddConnect(MockConnect(SYNCHRONOUS, ERR_FAILED), kIpV4Endpoint1);
  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  FastForwardBy(base::Milliseconds(5));
  request->add_endpoint(service_endpoint)
      .CallOnServiceEndpointRequestFinished(OK);
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());

  WaitForError(ERR_FAILED,
               /*expected_connection_attempts=*/{{kIpV4Endpoint1, ERR_FAILED}});
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
  CheckConnectTiming(/*dns_start=*/start_time_,
                     /*dns_end=*/base::TimeTicks::Now());
}

TEST_F(TcpConnectJobTest, ConnectionSuccessSyncDnsAsyncConnect) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  host_resolver_.ConfigureDefaultResolution()
      .add_endpoint(service_endpoint)
      .set_aliases(kDnsAliases)
      .CompleteStartSynchronously(OK);
  MockConnectCompleter connect_completer;
  AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);
  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  FastForwardBy(base::Milliseconds(6));
  connect_completer.WaitForConnectAndComplete(OK);

  WaitForSuccess(kIpV4Endpoint1, service_endpoint);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  EXPECT_EQ(kDnsAliases, test_delegate_->socket()->GetDnsAliases());
  CheckConnectTiming(/*dns_start=*/start_time_,
                     /*dns_end=*/start_time_);
}

TEST_F(TcpConnectJobTest, ConnectionErrorSyncDnsAsyncConnect) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  host_resolver_.ConfigureDefaultResolution()
      .add_endpoint(service_endpoint)
      .CompleteStartSynchronously(OK);
  MockConnectCompleter connect_completer;
  AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);
  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  FastForwardBy(base::Milliseconds(6));
  connect_completer.WaitForConnectAndComplete(ERR_FAILED);

  WaitForError(ERR_FAILED,
               /*expected_connection_attempts=*/{{kIpV4Endpoint1, ERR_FAILED}});
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
  CheckConnectTiming(/*dns_start=*/start_time_, /*dns_end=*/start_time_);
}

TEST_F(TcpConnectJobTest, ConnectionSuccessAsyncDnsAsyncConnect) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  auto request = host_resolver_.AddFakeRequest();
  MockConnectCompleter connect_completer;
  AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);
  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  FastForwardBy(base::Milliseconds(5));
  request->add_endpoint(service_endpoint)
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointRequestFinished(OK);
  base::TimeTicks dns_end = base::TimeTicks::Now();

  FastForwardBy(base::Milliseconds(6));
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
  connect_completer.WaitForConnectAndComplete(OK);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());

  WaitForSuccess(kIpV4Endpoint1, service_endpoint);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  EXPECT_EQ(kDnsAliases, test_delegate_->socket()->GetDnsAliases());
  CheckConnectTiming(/*dns_start=*/start_time_,
                     /*dns_end=*/dns_end);
}

TEST_F(TcpConnectJobTest, ConnectionErrorAsyncDnsAsyncConnect) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  auto request = host_resolver_.AddFakeRequest();
  MockConnectCompleter connect_completer;
  AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);
  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  FastForwardBy(base::Milliseconds(5));
  request->add_endpoint(service_endpoint)
      .CallOnServiceEndpointRequestFinished(OK);
  base::TimeTicks dns_end = base::TimeTicks::Now();

  FastForwardBy(base::Milliseconds(6));
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
  connect_completer.WaitForConnectAndComplete(ERR_FAILED);
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());

  WaitForError(ERR_FAILED,
               /*expected_connection_attempts=*/{{kIpV4Endpoint1, ERR_FAILED}});
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
  CheckConnectTiming(/*dns_start=*/start_time_,
                     /*dns_end=*/dns_end);
}

// Test the case where DNS never completes, but is crypto ready and provides an
// IP address, which should be enough for the job to connect and return a
// result.
TEST_F(TcpConnectJobTest, ConnectionSuccessAsyncPartialDnsSyncConnect) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  auto request = host_resolver_.AddFakeRequest();
  AddConnect(MockConnect(SYNCHRONOUS, OK), kIpV4Endpoint1);
  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  FastForwardBy(base::Milliseconds(5));
  request->set_crypto_ready(true)
      .add_endpoint(service_endpoint)
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();
  base::TimeTicks partial_dns_time = base::TimeTicks::Now();

  WaitForSuccess(kIpV4Endpoint1, service_endpoint);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  EXPECT_EQ(kDnsAliases, test_delegate_->socket()->GetDnsAliases());
  // Despite DNS not completing, DNS end time is when the endpoint was received,
  // at which point the ConnectJob was no longer blocked by DNS.
  CheckConnectTiming(/*dns_start=*/start_time_, /*dns_end=*/partial_dns_time);
}

TEST_F(TcpConnectJobTest, ConnectionSuccessAsyncPartialDnsAsyncConnect) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  auto request = host_resolver_.AddFakeRequest();
  MockConnectCompleter connect_completer;
  AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);
  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  FastForwardBy(base::Milliseconds(5));
  request->set_crypto_ready(true)
      .add_endpoint(service_endpoint)
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();
  base::TimeTicks partial_dns_time = base::TimeTicks::Now();

  FastForwardBy(base::Milliseconds(6));
  connect_completer.WaitForConnectAndComplete(OK);

  WaitForSuccess(kIpV4Endpoint1, service_endpoint);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  EXPECT_EQ(kDnsAliases, test_delegate_->socket()->GetDnsAliases());
  // Despite DNS not completing, DNS end time is when the endpoint was received,
  // at which point the ConnectJob was no longer blocked by DNS.
  CheckConnectTiming(/*dns_start=*/start_time_, /*dns_end=*/partial_dns_time);
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
    MockConnectCompleter connect_completer;
    AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);
    auto request = host_resolver_.AddFakeRequest();
    EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

    FastForwardBy(base::Milliseconds(5));
    request->add_endpoint(service_endpoint).CallOnServiceEndpointsUpdated();
    EXPECT_FALSE(connect_job_->HasEstablishedConnection());

    FastForwardBy(base::Milliseconds(6));
    connect_completer.WaitForConnectAndComplete(OK);
    EXPECT_TRUE(connect_job_->HasEstablishedConnection());
    request->set_crypto_ready(true).set_aliases(kDnsAliases);
    if (dns_completes) {
      request->CallOnServiceEndpointsUpdated();
    } else {
      request->CallOnServiceEndpointRequestFinished(OK);
    }

    WaitForSuccess(kIpV4Endpoint1, service_endpoint);
    EXPECT_EQ(kDnsAliases, test_delegate_->socket()->GetDnsAliases());
    EXPECT_TRUE(connect_job_->HasEstablishedConnection());

    // Since the ConnectJob was blocked on crypto ready after establishing a
    // connection, the request was effectively being blocked on DNS even while
    // connecting, so DNS time covers the entire duration of the request.
    CheckConnectTiming(/*dns_start=*/start_time_,
                       /*dns_end=*/base::TimeTicks::Now());
  }
}

// Just like the above test, but with an extra ServiceEndpointsUpdated() event
// before crypto ready is set. The request should not complete until crypto
// ready is set.
TEST_F(TcpConnectJobTest, ConnectionSuccessThenCryptoReady2) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  for (bool dns_completes : {true, false}) {
    SCOPED_TRACE(dns_completes);
    MockConnectCompleter connect_completer;
    AddConnect(MockConnect(&connect_completer), kIpV4Endpoint1);
    auto request = host_resolver_.AddFakeRequest();
    EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

    FastForwardBy(base::Milliseconds(5));
    request->add_endpoint(service_endpoint).CallOnServiceEndpointsUpdated();
    EXPECT_FALSE(connect_job_->HasEstablishedConnection());

    FastForwardBy(base::Milliseconds(6));
    connect_completer.WaitForConnectAndComplete(OK);
    EXPECT_TRUE(connect_job_->HasEstablishedConnection());

    // A superfluous OnServiceEndpointsUpdated() event. We could add more
    // endpoints, but it's not needed for this test. It should not cause the
    // request to complete. Spin the message loop by advancing time to make
    // sure there are no pending completion events.
    request->CallOnServiceEndpointsUpdated();
    FastForwardBy(base::Milliseconds(1));
    EXPECT_FALSE(test_delegate_->has_result());

    request->set_crypto_ready(true).set_aliases(kDnsAliases);
    if (dns_completes) {
      request->CallOnServiceEndpointsUpdated();
    } else {
      request->CallOnServiceEndpointRequestFinished(OK);
    }

    WaitForSuccess(kIpV4Endpoint1, service_endpoint);
    EXPECT_EQ(kDnsAliases, test_delegate_->socket()->GetDnsAliases());
    EXPECT_TRUE(connect_job_->HasEstablishedConnection());

    // Since the ConnectJob was blocked on crypto ready after establishing a
    // connection, the request was effectively being blocked on DNS even while
    // connecting, so DNS time covers the entire duration of the request.
    CheckConnectTiming(/*dns_start=*/start_time_,
                       /*dns_end=*/base::TimeTicks::Now());
  }
}

// Test the case where a connection is established, but it is rejected due to
// the HTTPS record, but there's a new IP that succeeds. In this case, DNS end
// should be the time the HTTPS record is received. This test is run both in the
// case where the DNS request completes, and where it doesn't, which shouldn't
// affect the results in any way.
TEST_F(TcpConnectJobTest, HttpsRecordRejectsConnectionSecondConnectionUsage) {
  const auto service_endpoint1 =
      CreateServiceEndpoint({kIpV6Endpoint1}, {"h2"}, /*ech=*/true);
  const auto service_endpoint2 = CreateServiceEndpoint({kIpV4Endpoint1});
  for (bool dns_completes : {true, false}) {
    SCOPED_TRACE(dns_completes);
    std::array<MockConnectCompleter, 2> connect_completers;
    AddConnect(MockConnect(&connect_completers[0]), kIpV4Endpoint1);
    AddConnect(MockConnect(&connect_completers[1]), kIpV6Endpoint1);
    auto request = host_resolver_.AddFakeRequest();
    EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

    FastForwardBy(base::Milliseconds(5));
    request->add_endpoint(service_endpoint2).CallOnServiceEndpointsUpdated();
    EXPECT_FALSE(connect_job_->HasEstablishedConnection());

    FastForwardBy(base::Milliseconds(6));
    connect_completers[0].WaitForConnectAndComplete(OK);
    EXPECT_TRUE(connect_job_->HasEstablishedConnection());

    FastForwardBy(base::Milliseconds(7));
    request->set_endpoints({service_endpoint1, service_endpoint2})
        .set_crypto_ready(true)
        .set_aliases(kDnsAliases);
    if (dns_completes) {
      request->CallOnServiceEndpointsUpdated();
    } else {
      request->CallOnServiceEndpointRequestFinished(OK);
    }
    base::TimeTicks crypto_ready_time = base::TimeTicks::Now();

    FastForwardBy(base::Milliseconds(8));
    connect_completers[1].WaitForConnectAndComplete(OK);

    WaitForSuccess(kIpV6Endpoint1, service_endpoint1);
    EXPECT_EQ(kDnsAliases, test_delegate_->socket()->GetDnsAliases());
    EXPECT_TRUE(connect_job_->HasEstablishedConnection());

    // The DNS time should include the time up to the crypto ready event, since
    // the request was blocked on that event.
    CheckConnectTiming(/*dns_start=*/start_time_,
                       /*dns_end=*/crypto_ready_time);
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

// Test the case when initially only one IP is received (and crypto ready),
// which fails. Then the connection attempt stalls until another IP is provided.
// The DNS connect end time should include the wait for the second IP.
TEST_F(TcpConnectJobTest, StalledAfterFirstIpFails) {
  const auto initial_service_endpoint = CreateServiceEndpoint({kIpV6Endpoint1});
  const auto final_service_endpoint =
      CreateServiceEndpoint({kIpV4Endpoint1, kIpV6Endpoint1});
  auto request = host_resolver_.AddFakeRequest();
  std::array<MockConnectCompleter, 2> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint1);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  request->add_endpoint(initial_service_endpoint)
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated();

  // First connection attempt fails, leaving no IPs to connect to, but the DNS
  // request is still going.
  FastForwardBy(base::Milliseconds(5));
  connect_completers[0].WaitForConnectAndComplete(ERR_FAILED);

  // The DNS request completes, providing a new IP.
  FastForwardBy(base::Milliseconds(5));
  request->set_endpoints({final_service_endpoint})
      .CallOnServiceEndpointRequestFinished(OK);
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
  base::TimeTicks final_sevice_endpoint_time = base::TimeTicks::Now();

  // Second IP succeeds.
  FastForwardBy(base::Milliseconds(5));
  connect_completers[1].WaitForConnectAndComplete(OK);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());

  WaitForSuccess(kIpV4Endpoint1,
                 final_service_endpoint, /*expected_connection_attempts=*/
                 {{kIpV6Endpoint1, ERR_FAILED}});

  // The DNS time should include the time up to when the second IP was received,
  // since the request was blocked on it.
  CheckConnectTiming(/*dns_start=*/start_time_,
                     /*dns_end=*/final_sevice_endpoint_time);
}

// Test the case when initially only one IP is received (and crypto ready),
// while that's connecting, the DNS lookup provide another IP. The first
// connection attempt fails, but the second succeeds. The DNS connect end time
// should not include the wait for the second IP, since we were busy connecting
// at the time.
TEST_F(TcpConnectJobTest, NotStalledAfterFirstIpFails) {
  const auto initial_service_endpoint = CreateServiceEndpoint({kIpV6Endpoint1});
  const auto final_service_endpoint =
      CreateServiceEndpoint({kIpV4Endpoint1, kIpV6Endpoint1});
  auto request = host_resolver_.AddFakeRequest();
  std::array<MockConnectCompleter, 2> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint1);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  request->add_endpoint(initial_service_endpoint)
      .set_crypto_ready(true)
      .CallOnServiceEndpointsUpdated();

  // The DNS request completes, providing a new IP.
  FastForwardBy(base::Milliseconds(5));
  request->set_endpoints({final_service_endpoint})
      .CallOnServiceEndpointRequestFinished(OK);
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());

  // First connection attempt fails.
  FastForwardBy(base::Milliseconds(5));
  connect_completers[0].WaitForConnectAndComplete(ERR_FAILED);

  // Second IP succeeds.
  FastForwardBy(base::Milliseconds(5));
  connect_completers[1].WaitForConnectAndComplete(OK);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());

  WaitForSuccess(kIpV4Endpoint1,
                 final_service_endpoint, /*expected_connection_attempts=*/
                 {{kIpV6Endpoint1, ERR_FAILED}});

  // The DNS time should not include the time up to when the second IP was
  // received, since the request was never blocked waiting on it.
  CheckConnectTiming(/*dns_start=*/start_time_,
                     /*dns_end=*/start_time_);
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

// This tests the case where there are a bunch of OnHostResolutionCallback
// results of kMayBeDeletedAsync received at once (with one kContinue mixed in).
// establishing a connection should be delayed until all callbacks have been
// run. This test doesn't actually check when connection establishment takes
// place (if it happens too early, we'll just start connecting instead of
// waiting for a potential deletion), but it does make sure that the ConnectJob
// doesn't stall indefinitely, or CHECK if that happens.
TEST_F(TcpConnectJobTest,
       OnHostResolutionCallbackMultipleMayBeDeletedAsyncCallsInARowButItIsNot) {
  const auto service_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  auto request = host_resolver_.AddFakeRequest();
  AddConnect(MockConnect(ASYNC, OK), kIpV4Endpoint1);

  bool callback_run = false;
  EnableHostResolutionCallbacks(
      {OnHostResolutionCallbackResult::kMayBeDeletedAsync,
       OnHostResolutionCallbackResult::kMayBeDeletedAsync,
       OnHostResolutionCallbackResult::kContinue,
       OnHostResolutionCallbackResult::kMayBeDeletedAsync,
       OnHostResolutionCallbackResult::kMayBeDeletedAsync},
      base::BindLambdaForTesting([&]() {
        // The ConnectJob will only continue after another PostTask triggered by
        // the last `kMayBeDeletedAsync` result has been executed, which should
        // not have happened yet, since this task should be posted immediately
        // before the PostTask triggered by the last `kMayBeDeletedAsync`.
        EXPECT_FALSE(client_socket_factory_.AllDataProvidersUsed());
        callback_run = true;
      }));

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // Add the endpoint, provide a number of CallOnServiceEndpointsUpdated() calls
  // without modifying the data at all, and then complete the DNS request.
  request->add_endpoint(service_endpoint).set_aliases(kDnsAliases);
  for (int i = 0; i < 4; ++i) {
    request->CallOnServiceEndpointsUpdated();
  }
  request->CallOnServiceEndpointRequestFinished(OK);

  WaitForSuccess(kIpV4Endpoint1, service_endpoint);
  const HostResolutionCallbackInfo expected_host_resolution_info{
      {service_endpoint}, kDnsAliases};
  EXPECT_THAT(host_resolution_callback_info_,
              testing::ElementsAre(
                  expected_host_resolution_info, expected_host_resolution_info,
                  expected_host_resolution_info, expected_host_resolution_info,
                  expected_host_resolution_info));
  EXPECT_TRUE(callback_run);
  EXPECT_TRUE(client_socket_factory_.AllDataProvidersUsed());
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

  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());
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
  CheckConnectTiming(/*dns_start=*/start_time_,
                     /*dns_end=*/base::TimeTicks::Now());
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
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());
  FastForwardBy(TcpConnectJob::kIPv6FallbackTime);
  EXPECT_EQ(2u, connect_job_->GetFreshConnectorCountForTesting());

  connect_completer.Complete(OK);
  WaitForSuccess(kIpV4Endpoint1, service_endpoint);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  CheckConnectTiming(/*dns_start=*/start_time_, /*dns_end=*/start_time_);
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
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());
  FastForwardBy(TcpConnectJob::kIPv6FallbackTime);
  EXPECT_EQ(2u, connect_job_->GetFreshConnectorCountForTesting());

  connect_completer.Complete(ERR_FAILED);
  WaitForError(ERR_FAILED,
               /*expected_connection_attempts=*/{{kIpV4Endpoint1, ERR_FAILED}});
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
  CheckConnectTiming(/*dns_start=*/start_time_, /*dns_end=*/start_time_);
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
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());
  FastForwardBy(TcpConnectJob::kIPv6FallbackTime);
  EXPECT_EQ(2u, connect_job_->GetFreshConnectorCountForTesting());

  connect_completer.Complete(OK);
  WaitForSuccess(
      kIpV4Endpoint1, service_endpoint,
      /*expected_connection_attempts=*/{{kIpV6Endpoint1, ERR_FAILED}});
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  CheckConnectTiming(/*dns_start=*/start_time_, /*dns_end=*/start_time_);
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
    EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());
    EXPECT_EQ(base::Time::Now() - start_time, base::TimeDelta());

    // Wait for the second Connector to start, which should mean the fallback
    // time has passed.
    connect_completers[1].WaitForConnect();
    EXPECT_EQ(2u, connect_job_->GetFreshConnectorCountForTesting());
    EXPECT_EQ(base::Time::Now() - start_time, TcpConnectJob::kIPv6FallbackTime);

    connect_completers[successful_index].Complete(OK);

    WaitForSuccess(successful_index == 0 ? kIpV6Endpoint1 : kIpV4Endpoint1,
                   service_endpoint);
    CheckConnectTiming(/*dns_start=*/start_time_, /*dns_end=*/start_time_);
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
    EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());
    EXPECT_EQ(base::Time::Now() - start_time, base::TimeDelta());

    // Wait for the second Connector to start, which should mean the fallback
    // time has passed.
    connect_completers[1].WaitForConnect();
    EXPECT_EQ(2u, connect_job_->GetFreshConnectorCountForTesting());
    EXPECT_EQ(base::Time::Now() - start_time, TcpConnectJob::kIPv6FallbackTime);

    connect_completers[1 - successful_index].Complete(ERR_FAILED);
    connect_completers[successful_index].Complete(OK);

    WaitForSuccess(successful_index == 0 ? kIpV6Endpoint1 : kIpV4Endpoint1,
                   service_endpoint,
                   /*expected_connection_attempts=*/
                   {{successful_index == 0 ? kIpV4Endpoint1 : kIpV6Endpoint1,
                     ERR_FAILED}});
    CheckConnectTiming(/*dns_start=*/start_time_, /*dns_end=*/start_time_);
  }
}

// Test the case where two connectors are blocked on crypto ready. In this case,
// DNS end time includes time up to receiving crypto ready.
TEST_F(TcpConnectJobTest, TwoConnectorsBlockedOnCryptoReady) {
  const auto service_endpoint =
      CreateServiceEndpoint({kIpV4Endpoint1, kIpV6Endpoint1});

  auto request = host_resolver_.AddFakeRequest();
  std::array<MockConnectCompleter, 2> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint1);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // The A and AAAA resolutions complete.
  FastForwardBy(base::Milliseconds(5));
  request->add_endpoint(service_endpoint).CallOnServiceEndpointsUpdated();

  FastForwardBy(base::Milliseconds(5));
  connect_completers[0].WaitForConnect();
  connect_completers[1].WaitForConnect();
  EXPECT_EQ(2u, connect_job_->GetFreshConnectorCountForTesting());

  // Both connections complete, but have to wait for crypto ready.
  connect_completers[0].Complete(OK);
  connect_completers[1].Complete(OK);

  // Receive crypto ready a little time later.
  FastForwardBy(base::Milliseconds(5));
  request->CallOnServiceEndpointRequestFinished(OK);

  // Request completes. We prefer the primary connector's socket, which is the
  // IPv6 socket.
  WaitForSuccess(kIpV6Endpoint1, service_endpoint);
  // Since both requests were blocked on getting an IP, the DNS end time is when
  // the crypto ready event was received.
  CheckConnectTiming(/*dns_start=*/start_time_,
                     /*dns_end=*/base::TimeTicks::Now());
}

// Test the case where one connector is blocked on crypto ready, while the other
// is connecting, when crypto ready occurs. In this case, DNS end time does not
// include time up to receiving crypto ready, though perhaps it should. The
// reason for not doing so if because of the case where, e.g., there's one
// connecting IP and two connectors waiting on a second IP, where we shouldn't
// update DNS time on crypto ready.
TEST_F(TcpConnectJobTest, TwoConnectorsOneBlockedOnCryptoReady) {
  const auto service_endpoint =
      CreateServiceEndpoint({kIpV4Endpoint1, kIpV6Endpoint1});

  for (int endpoint_to_connect : {0, 1}) {
    SCOPED_TRACE(endpoint_to_connect);
    auto request = host_resolver_.AddFakeRequest();
    std::array<MockConnectCompleter, 2> connect_completers;
    AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
    AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint1);

    EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

    // The A and AAAA resolutions complete.
    request->add_endpoint(service_endpoint).CallOnServiceEndpointsUpdated();

    FastForwardBy(base::Milliseconds(5));
    connect_completers[0].WaitForConnect();
    connect_completers[1].WaitForConnect();
    EXPECT_EQ(2u, connect_job_->GetFreshConnectorCountForTesting());

    // One connection completes, but has to wait for crypto ready.
    connect_completers[endpoint_to_connect].Complete(OK);

    // Receive crypto ready a little time later.
    FastForwardBy(base::Milliseconds(5));
    request->CallOnServiceEndpointRequestFinished(OK);

    // Request completes. We prefer the primary connector's socket, which is the
    // IPv6 socket.
    WaitForSuccess(endpoint_to_connect == 0 ? kIpV6Endpoint1 : kIpV4Endpoint1,
                   service_endpoint);
    // Since only one request was blocked on getting an IP, the DNS end time
    // does not include the wait for crypto ready.
    CheckConnectTiming(/*dns_start=*/start_time_,
                       /*dns_end=*/start_time_);
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
    EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());
    EXPECT_EQ(base::Time::Now() - start_time, base::TimeDelta());

    // Wait for the second Connector to start, which should mean the fallback
    // time has passed.
    connect_completers[1].WaitForConnect();
    EXPECT_EQ(2u, connect_job_->GetFreshConnectorCountForTesting());
    EXPECT_EQ(base::Time::Now() - start_time, TcpConnectJob::kIPv6FallbackTime);

    connect_completers[first_failure].Complete(errors[first_failure]);
    connect_completers[second_failure].Complete(errors[second_failure]);

    WaitForError(errors[second_failure],
                 /*expected_connection_attempts=*/{
                     {endpoints[first_failure], errors[first_failure]},
                     {endpoints[second_failure], errors[second_failure]}});
    CheckConnectTiming(/*dns_start=*/start_time_, /*dns_end=*/start_time_);
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
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());
  EXPECT_EQ(base::Time::Now() - start_time, base::TimeDelta());

  // Wait for the second Connector to start, which should mean the fallback time
  // has passed.
  connect_completers[1].WaitForConnect();
  EXPECT_EQ(2u, connect_job_->GetFreshConnectorCountForTesting());
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
// and fail and only then do the IPv4 ones come in. Only the primary connector
// should try IPv6, and only the secondary IPv4, since the DNS lookup doesn't
// complete until we're trying the last IPv4 endpoint. There's only a single
// ServiceEndpoint in this test, though it's updated half-way through.
TEST_F(TcpConnectJobTest, TwoConnectorsIPv6ThenIpv4) {
  const auto service_endpoint =
      CreateServiceEndpoint({kIpV6Endpoint1, kIpV6Endpoint2, kIpV6Endpoint3,
                             kIpV4Endpoint1, kIpV4Endpoint2, kIpV4Endpoint3});

  base::Time start_time = base::Time::Now();

  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 8> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV6Endpoint2);
  AddConnect(MockConnect(&connect_completers[2]), kIpV6Endpoint3);
  AddConnect(MockConnect(&connect_completers[3]), kIpV4Endpoint1);
  AddConnect(MockConnect(&connect_completers[4]), kIpV4Endpoint2);
  AddConnect(MockConnect(&connect_completers[5]), kIpV4Endpoint3);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));
  // Temporary endpoint that only includes AAAA results.
  request
      ->add_endpoint(CreateServiceEndpoint(
          {kIpV6Endpoint1, kIpV6Endpoint2, kIpV6Endpoint3}))
      .CallOnServiceEndpointsUpdated();

  connect_completers[0].WaitForConnect();
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());
  EXPECT_EQ(base::Time::Now() - start_time, base::TimeDelta());

  // Wait for the IPv4 job to start.
  FastForwardBy(TcpConnectJob::kIPv6FallbackTime);
  EXPECT_EQ(2u, connect_job_->GetFreshConnectorCountForTesting());
  // Since only IPv6 IPs are available, it should not connect.
  EXPECT_FALSE(connect_completers[1].is_connecting());

  // The IPv6 endpoints fail one at a time. The primary connector should advance
  // through them, while the IPv4 connector is still stalled.
  connect_completers[0].Complete(ERR_FAILED);
  connect_completers[1].WaitForConnect();
  EXPECT_FALSE(connect_completers[2].is_connecting());
  connect_completers[1].Complete(ERR_FAILED);
  connect_completers[2].WaitForConnect();
  EXPECT_FALSE(connect_completers[3].is_connecting());
  connect_completers[2].Complete(ERR_FAILED);
  EXPECT_FALSE(connect_completers[3].is_connecting());

  // At this point, we're stalled waiting for more IPs.

  // IPv4 IPs come in, but DNS does not yet complete.
  request->set_endpoints({service_endpoint}).CallOnServiceEndpointsUpdated();

  // Now the IPv4 connector tries the IPv4 IPs one-at-a-time.
  connect_completers[3].WaitForConnect();
  EXPECT_FALSE(connect_completers[4].is_connecting());
  connect_completers[3].Complete(ERR_UNEXPECTED);
  connect_completers[4].WaitForConnect();
  EXPECT_FALSE(connect_completers[5].is_connecting());
  connect_completers[4].Complete(ERR_UNEXPECTED);

  // The final connector completes.
  connect_completers[5].WaitForConnectAndComplete(OK);
  EXPECT_FALSE(test_delegate_->has_result());

  // The DNS lookup completes, which should complete the request.
  request->CallOnServiceEndpointRequestFinished(OK);

  WaitForSuccess(kIpV4Endpoint3, service_endpoint,
                 /*expected_connection_attempts=*/
                 {{kIpV6Endpoint1, ERR_FAILED},
                  {kIpV6Endpoint2, ERR_FAILED},
                  {kIpV6Endpoint3, ERR_FAILED},
                  {kIpV4Endpoint1, ERR_UNEXPECTED},
                  {kIpV4Endpoint2, ERR_UNEXPECTED}});
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
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());
  EXPECT_EQ(base::Time::Now() - start_time, base::TimeDelta());

  // Wait for the second Connector to start, which should mean the fallback time
  // has passed.
  connect_completers[1].WaitForConnect();
  EXPECT_EQ(2u, connect_job_->GetFreshConnectorCountForTesting());
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
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());
  EXPECT_EQ(base::Time::Now() - start_time, base::TimeDelta());

  // Wait for the second Connector to start, which should mean the fallback time
  // has passed.
  connect_completers[2].WaitForConnect();
  EXPECT_EQ(2u, connect_job_->GetFreshConnectorCountForTesting());
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

// Test the case where there are two connectors with multiple ServiceEndpoints,
// and each connector blocks waiting for the other to complete, when the other
// connector has IPs that the blocked on cannot try.
TEST_F(TcpConnectJobTest, TwoConnectorsBlockedOnOtherConnector) {
  const auto service_endpoint1 =
      CreateServiceEndpoint({kIpV6Endpoint1, kIpV6Endpoint2});
  const auto service_endpoint2 =
      CreateServiceEndpoint({kIpV4Endpoint1, kIpV4Endpoint2});
  const auto service_endpoint3 =
      CreateServiceEndpoint({kIpV6Endpoint3, kIpV4Endpoint3});

  base::Time start_time = base::Time::Now();

  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 8> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV6Endpoint2);
  AddConnect(MockConnect(&connect_completers[2]), kIpV4Endpoint1);
  AddConnect(MockConnect(&connect_completers[3]), kIpV4Endpoint2);
  AddConnect(MockConnect(&connect_completers[4]), kIpV4Endpoint3);
  AddConnect(MockConnect(&connect_completers[5]), kIpV6Endpoint3);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));
  // Add all endpoints, but the DNS lookup does not complete.
  request
      ->set_endpoints({service_endpoint1, service_endpoint2, service_endpoint3})
      .CallOnServiceEndpointsUpdated();

  connect_completers[0].WaitForConnect();
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());
  EXPECT_EQ(base::Time::Now() - start_time, base::TimeDelta());

  // Wait for the IPv4 job to start.
  FastForwardBy(TcpConnectJob::kIPv6FallbackTime);
  EXPECT_EQ(2u, connect_job_->GetFreshConnectorCountForTesting());
  // Since only IPv6 IPs are available, it should not connect.
  EXPECT_FALSE(connect_completers[1].is_connecting());

  // The first two IPv6 endpoints fail one at a time. Only the primary connector
  // should be connecting.
  connect_completers[0].Complete(ERR_FAILED);
  connect_completers[1].WaitForConnect();
  EXPECT_FALSE(connect_completers[2].is_connecting());
  connect_completers[1].Complete(ERR_UNEXPECTED);

  // After the last IPv6 IP from `service_endpoint1` fails, we switch to
  // `service_endpoint2`, and now only the IPv4 connector tries to connect.
  connect_completers[2].WaitForConnect();
  EXPECT_FALSE(connect_completers[3].is_connecting());
  connect_completers[2].Complete(ERR_FAILED);
  connect_completers[3].WaitForConnect();
  EXPECT_FALSE(connect_completers[4].is_connecting());
  EXPECT_FALSE(connect_completers[5].is_connecting());
  connect_completers[3].Complete(ERR_UNEXPECTED);

  // Now that both endpoints in `service_endpoint2` have failed, we advance to
  // `service_endpoint3`. Both connectors can now try endpoints, since both an
  // IPv4 and IPv6 endpoint is available.
  connect_completers[4].WaitForConnect();
  connect_completers[5].WaitForConnect();

  // The DNS lookup completes, as does one of the connectors, which should
  // complete the job.
  request->CallOnServiceEndpointRequestFinished(OK);
  connect_completers[5].Complete(OK);

  WaitForSuccess(kIpV6Endpoint3, service_endpoint3,
                 /*expected_connection_attempts=*/
                 {{kIpV6Endpoint1, ERR_FAILED},
                  {kIpV6Endpoint2, ERR_UNEXPECTED},
                  {kIpV4Endpoint1, ERR_FAILED},
                  {kIpV4Endpoint2, ERR_UNEXPECTED}});
  // No more time should have passed since the second connector was started,
  // since time wasn't simulated advancing, and there should have been no other
  // timed delay by TcpConnectJob.
  EXPECT_EQ(base::Time::Now() - start_time, TcpConnectJob::kIPv6FallbackTime);
}

// Just like the above test, but the DNS request completes immediately, rather
// than being stalled after returning endpoints, so both jobs can connect to
// IPv4/IPv6 destinations, if there are no destinations of the matching type
// available.
TEST_F(TcpConnectJobTest, TwoConnectorsNotBlockedOnOtherConnector) {
  const auto service_endpoint1 =
      CreateServiceEndpoint({kIpV6Endpoint1, kIpV6Endpoint2});
  const auto service_endpoint2 =
      CreateServiceEndpoint({kIpV4Endpoint1, kIpV4Endpoint2});
  const auto service_endpoint3 =
      CreateServiceEndpoint({kIpV6Endpoint3, kIpV4Endpoint3});

  base::Time start_time = base::Time::Now();

  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 8> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV6Endpoint2);
  AddConnect(MockConnect(&connect_completers[2]), kIpV4Endpoint1);
  AddConnect(MockConnect(&connect_completers[3]), kIpV4Endpoint2);
  AddConnect(MockConnect(&connect_completers[4]), kIpV6Endpoint3);
  AddConnect(MockConnect(&connect_completers[5]), kIpV4Endpoint3);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));
  // Add all endpoints, but the DNS lookup does not complete.
  request
      ->set_endpoints({service_endpoint1, service_endpoint2, service_endpoint3})
      .CallOnServiceEndpointRequestFinished(OK);

  connect_completers[0].WaitForConnect();
  EXPECT_FALSE(connect_completers[1].is_connecting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());
  EXPECT_EQ(base::Time::Now() - start_time, base::TimeDelta());

  // Wait for the IPv4 connector to start.
  connect_completers[1].WaitForConnect();
  EXPECT_EQ(base::Time::Now() - start_time, TcpConnectJob::kIPv6FallbackTime);

  // The first IPv6 destination fails. Primary connector should stall, waiting
  // for the "IPv4" connector to fail before advancing to the next
  // ServiceEndpoint.
  connect_completers[0].Complete(ERR_FAILED);
  EXPECT_FALSE(connect_completers[2].is_connecting());
  EXPECT_FALSE(connect_completers[3].is_connecting());

  // Second IPv6 destination fails. Both connectors should start connecting to
  // `service_endpoint2` IPs.
  connect_completers[1].Complete(ERR_UNEXPECTED);
  connect_completers[2].WaitForConnect();
  connect_completers[3].WaitForConnect();

  // The first IPv4 destination fails. One connector (the IPv4 one) should
  // stall, waiting for the other connector to fail before advancing to the next
  // ServiceEndpoint.
  connect_completers[2].Complete(ERR_FAILED);
  EXPECT_FALSE(connect_completers[4].is_connecting());
  EXPECT_FALSE(connect_completers[5].is_connecting());

  // Second IPv4 destination fails. Both connectors should start connecting to
  // `service_endpoint3` IPs. The IPv6 one gets the IPv6 IP first, but that's
  // not a major detail.
  connect_completers[3].Complete(ERR_UNEXPECTED);
  connect_completers[4].WaitForConnect();
  connect_completers[5].WaitForConnect();

  // One connector completes successfully, ending the job.
  connect_completers[5].Complete(OK);

  WaitForSuccess(kIpV4Endpoint3, service_endpoint3,
                 /*expected_connection_attempts=*/
                 {{kIpV6Endpoint1, ERR_FAILED},
                  {kIpV6Endpoint2, ERR_UNEXPECTED},
                  {kIpV4Endpoint1, ERR_FAILED},
                  {kIpV4Endpoint2, ERR_UNEXPECTED}});
  // No more time should have passed since the second connector was started,
  // since time wasn't simulated advancing, and there should have been no other
  // timed delay by TcpConnectJob.
  EXPECT_EQ(base::Time::Now() - start_time, TcpConnectJob::kIPv6FallbackTime);
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
  EXPECT_EQ(2u, connect_job_->GetFreshConnectorCountForTesting());
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

class TcpConnectJobRTTFallbackTest : public TcpConnectJobTest {
 public:
  TcpConnectJobRTTFallbackTest()
      : TcpConnectJobTest(
            /*enabled_features=*/
            {{features::kHappyEyeballsV2, {}},
             {features::kIPv6FallbackBasedOnRTT,
              {{"IPv6FallbackRTTMultiplier", "2.0"},
               {"IPv6FallbackMin", "10ms"},
               {"IPv6FallbackMax", "1s"}}}},
            /*disabled_features=*/{}) {}
};

TEST_F(TcpConnectJobRTTFallbackTest, UsesRTTForFallback) {
  // Set up HttpServerProperties with a specific RTT.
  url::SchemeHostPort server(url::kHttpsScheme, kHostName, 443);
  ServerNetworkStats stats;
  stats.srtt = base::Milliseconds(50);
  http_server_properties_.SetServerNetworkStats(
      server, NetworkAnonymizationKey(), stats);

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
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // RTT is 50ms, multiplier is 2.0, so fallback should be 100ms.
  FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(2u, connect_job_->GetFreshConnectorCountForTesting());

  connect_completer.Complete(OK);
  WaitForSuccess(
      kIpV4Endpoint1, service_endpoint,
      /*expected_connection_attempts=*/{{kIpV6Endpoint1, ERR_FAILED}});
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  CheckConnectTiming(/*dns_start=*/start_time_, /*dns_end=*/start_time_);
}

class TcpConnectJobOptimisticDnsTest
    : public TcpConnectJobTest,
      public testing::WithParamInterface<bool> {
 public:
  TcpConnectJobOptimisticDnsTest()
      : TcpConnectJobTest(
            {{features::kOptimisticDnsForTcp,
              {{features::kUseStaleConnectorsForOptimisticDns.name,
                GetParam() ? "true" : "false"}}},
             {features::kHappyEyeballsV2, {}}},
            {}) {}

  bool UseStaleConnectors() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All, TcpConnectJobOptimisticDnsTest, testing::Bool());

TEST_P(TcpConnectJobOptimisticDnsTest, OptimisticDnsRequiresTlsEnabled) {
  const auto fresh_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 1> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV4Endpoint1);

  // Explicitly disable stale DNS to simulate a plain text connection (or a
  // TLS connection retry) where optimistic DNS is disallowed.
  disable_stale_dns_ = true;
  InitConnectJob();
  EXPECT_THAT(connect_job_->Connect(), IsError(ERR_IO_PENDING));

  // The request should not have requested stale results because it is not for
  // TLS.
  EXPECT_NE(request->resolve_host_params().cache_usage,
            HostResolver::ResolveHostParameters::CacheUsage::
                STALE_ALLOWED_WHILE_REFRESHING);

  // Since it didn't request stale endpoints, the HostResolver will only
  // provide fresh endpoints.
  request->set_crypto_ready(true)
      .set_is_stale_while_refreshing(false)
      .set_endpoints({fresh_endpoint})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(connect_completers[0].is_connecting());

  request->CallOnServiceEndpointRequestFinished(OK);

  connect_completers[0].Complete(OK);
  CheckConnection(kIpV4Endpoint1, fresh_endpoint);
}

TEST_P(TcpConnectJobOptimisticDnsTest, StaleAThenFreshA) {
  const auto stale_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  const auto fresh_endpoint = CreateServiceEndpoint({kIpV4Endpoint2});
  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 2> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV4Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint2);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  EXPECT_EQ(request->resolve_host_params().cache_usage,
            HostResolver::ResolveHostParameters::CacheUsage::
                STALE_ALLOWED_WHILE_REFRESHING);

  // t=0: Stale A provided
  request->set_crypto_ready(true)
      .set_is_stale_while_refreshing(true)
      .set_endpoints({stale_endpoint})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_FALSE(connect_completers[1].is_connecting());

  // t=50ms: Fresh A provided.
  constexpr base::TimeDelta kFreshAArrivalTime = base::Milliseconds(50);
  CHECK_LT(kFreshAArrivalTime, TcpConnectJob::kIPv6FallbackTime);
  FastForwardBy(kFreshAArrivalTime);
  request->set_is_stale_while_refreshing(false)
      .set_endpoints({fresh_endpoint})
      .CallOnServiceEndpointsUpdated();

  if (!UseStaleConnectors()) {
    // The TCPConnectJob should not immediately connect to the Fresh A
    // because it is waiting for Stale A to complete or the slow_timer_ (300ms)
    // to fire.
    EXPECT_TRUE(connect_completers[0].is_connecting());
    EXPECT_FALSE(connect_completers[1].is_connecting());

    // Fast forward to t=299ms. Still no new connection.
    FastForwardBy(TcpConnectJob::kIPv6FallbackTime - kFreshAArrivalTime -
                  base::Milliseconds(1));
    EXPECT_FALSE(connect_completers[1].is_connecting());

    // Fast forward to t=300ms. Now the slow_timer_ fires.
    FastForwardBy(base::Milliseconds(1));

    // Since DNS is not complete, the second connector is restricted to IPv6.
    // Fresh A is IPv4, so it is ignored.
    EXPECT_FALSE(connect_completers[1].is_connecting());

    // Now, DNS request completes.
    request->CallOnServiceEndpointRequestFinished(OK);

    // Now that DNS is complete, the second connector is no longer restricted to
    // IPv6. It should pick up Fresh A and connect.
    EXPECT_TRUE(connect_completers[1].is_connecting());
  } else {
    // With dual race, fresh endpoints run in fresh_state_ concurrently.
    // Fresh A should start immediately because fresh_state_ has no active
    // attempts.
    EXPECT_TRUE(connect_completers[0].is_connecting());
    EXPECT_TRUE(connect_completers[1].is_connecting());

    FastForwardBy(TcpConnectJob::kIPv6FallbackTime - kFreshAArrivalTime);
    request->CallOnServiceEndpointRequestFinished(OK);
    EXPECT_TRUE(connect_completers[0].is_connecting());
    EXPECT_TRUE(connect_completers[1].is_connecting());
  }

  // Complete Fresh A.
  connect_completers[1].Complete(OK);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  WaitForSuccess(kIpV4Endpoint2, fresh_endpoint);
}

TEST_P(TcpConnectJobOptimisticDnsTest,
       StalePrimaryPromoted_StaleIPv4ContinuesConnecting) {
  // Connector promotion only applies when dual-race mode is enabled.
  if (!UseStaleConnectors()) {
    GTEST_SKIP()
        << "Connector promotion only applies when dual-race mode is enabled.";
  }

  const auto stale_endpoint =
      CreateServiceEndpoint({kIpV6Endpoint1, kIpV4Endpoint1});
  const auto fresh_endpoint =
      CreateServiceEndpoint({kIpV6Endpoint1, kIpV4Endpoint2});
  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 3> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]),
             kIpV6Endpoint1);  // Stale AAAA
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint1);  // Stale A
  AddConnect(MockConnect(&connect_completers[2]), kIpV4Endpoint2);  // Fresh A

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // t=0: Stale endpoints provided
  request->set_crypto_ready(true)
      .set_is_stale_while_refreshing(true)
      .set_endpoints({stale_endpoint})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(connect_completers[0].is_connecting());   // Stale AAAA starts
  EXPECT_FALSE(connect_completers[1].is_connecting());  // Stale A is queued
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // t=300ms: slow_timer_ fires.
  FastForwardBy(TcpConnectJob::kIPv6FallbackTime);
  EXPECT_TRUE(connect_completers[1].is_connecting());  // Stale A starts
  EXPECT_EQ(2u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // Now fresh DNS arrives with only Stale AAAA and Fresh A
  request->set_is_stale_while_refreshing(false)
      .set_endpoints({fresh_endpoint})
      .CallOnServiceEndpointsUpdated();

  // Stale AAAA is promoted to fresh_state_.primary_connector.
  // Stale A stays in stale_state_.ipv4_connector.
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // Now we fail fresh_state_.primary_connector (Stale AAAA).
  connect_completers[0].Complete(ERR_CONNECTION_REFUSED);

  // Fresh A starts connecting.
  EXPECT_TRUE(connect_completers[2].is_connecting());

  // We fail Fresh A as well, meaning all fresh endpoints are exhausted.
  connect_completers[2].Complete(ERR_CONNECTION_REFUSED);

  // Complete the DNS request. The connect job should not complete because
  // stale_state_.ipv4_connector (Stale A) is still running.
  request->CallOnServiceEndpointRequestFinished(OK);
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
  EXPECT_FALSE(test_delegate_->has_result());

  // Now we complete Stale A with OK.
  connect_completers[1].Complete(OK);

  // The job should now succeed.
  WaitForSuccess(kIpV4Endpoint1, stale_endpoint,
                 {ConnectionAttempt(kIpV6Endpoint1, ERR_CONNECTION_REFUSED),
                  ConnectionAttempt(kIpV4Endpoint2, ERR_CONNECTION_REFUSED)});
}

TEST_P(TcpConnectJobOptimisticDnsTest,
       StaleSlowTimerNotRestartedAfterFreshDNSArrives) {
  // This test only makes sense when dual-race mode is enabled.
  if (!UseStaleConnectors()) {
    GTEST_SKIP()
        << "Connector promotion only applies when dual-race mode is enabled.";
  }

  const auto stale_endpoint =
      CreateServiceEndpoint({kIpV6Endpoint1, kIpV6Endpoint2, kIpV4Endpoint1});
  const auto fresh_endpoint = CreateServiceEndpoint({kIpV4Endpoint2});
  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 2> connect_completers;
  // Order of connection attempts:
  // 1. Stale AAAA
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  // 2. Fresh A
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint2);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // t=0: Stale endpoints provided
  request->set_crypto_ready(true)
      .set_is_stale_while_refreshing(true)
      .set_endpoints({stale_endpoint})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(connect_completers[0].is_connecting());  // Stale AAAA starts

  // Now fresh DNS arrives with only Fresh A
  request->set_is_stale_while_refreshing(false)
      .set_endpoints({fresh_endpoint})
      .CallOnServiceEndpointsUpdated();

  // Fresh A starts connecting.
  EXPECT_TRUE(connect_completers[1].is_connecting());

  // Wait for the slow timer duration.
  // Because the slow timer was stopped by fresh DNS and should not be
  // restarted, Stale A should not start.
  FastForwardBy(TcpConnectJob::kIPv6FallbackTime);

  // Complete the DNS request.
  request->CallOnServiceEndpointRequestFinished(OK);

  // Succeed Fresh A.
  connect_completers[1].Complete(OK);

  // Stale AAAA is still connecting, but the job completes successfully because
  // Fresh A succeeded.
  WaitForSuccess(kIpV4Endpoint2, fresh_endpoint);
}

TEST_P(TcpConnectJobOptimisticDnsTest, StaleAThenFreshAAAAThenFreshA) {
  const auto stale_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  const auto fresh_endpoint_v6 = CreateServiceEndpoint({kIpV6Endpoint1});
  const auto fresh_endpoint_merged =
      CreateServiceEndpoint({kIpV6Endpoint1, kIpV4Endpoint2});
  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 3> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV4Endpoint1);  // Stale A
  AddConnect(MockConnect(&connect_completers[1]),
             kIpV6Endpoint1);  // Fresh AAAA
  AddConnect(MockConnect(&connect_completers[2]), kIpV4Endpoint2);  // Fresh A

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // t=0: Stale A provided
  request->set_crypto_ready(true)
      .set_is_stale_while_refreshing(true)
      .set_endpoints({stale_endpoint})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_FALSE(connect_completers[1].is_connecting());
  EXPECT_FALSE(connect_completers[2].is_connecting());
  if (GetParam()) {
    EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
    EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());
  }

  // t=15ms: Fresh AAAA provided.
  constexpr base::TimeDelta kFreshAAAAArrivalTime = base::Milliseconds(15);
  CHECK_LT(kFreshAAAAArrivalTime, TcpConnectJob::kIPv6FallbackTime);
  FastForwardBy(kFreshAAAAArrivalTime);
  request->set_is_stale_while_refreshing(false)
      .set_endpoints({fresh_endpoint_v6})
      .CallOnServiceEndpointsUpdated();

  if (!UseStaleConnectors()) {
    EXPECT_FALSE(connect_completers[1].is_connecting());
  } else {
    // Fresh AAAA starts immediately in fresh_state_.
    EXPECT_TRUE(connect_completers[1].is_connecting());
    EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
    EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());
  }

  // t=20ms: Fresh A provided.
  constexpr base::TimeDelta kFreshAArrivalTime = base::Milliseconds(20);
  CHECK_LT(kFreshAArrivalTime, TcpConnectJob::kIPv6FallbackTime);
  FastForwardBy(kFreshAArrivalTime - kFreshAAAAArrivalTime);
  request->set_is_stale_while_refreshing(false)
      .set_endpoints({fresh_endpoint_merged})
      .CallOnServiceEndpointsUpdated();

  EXPECT_FALSE(connect_completers[2].is_connecting());

  if (!UseStaleConnectors()) {
    // t=300ms: slow_timer_ fires. It should start connecting to the preferred
    // Fresh AAAA.
    FastForwardBy(TcpConnectJob::kIPv6FallbackTime - kFreshAArrivalTime);
    EXPECT_TRUE(connect_completers[1].is_connecting());

    // t=600ms: another slow_timer_ fires, but TcpConnectJob only supports 2
    // concurrent connections.
    FastForwardBy(TcpConnectJob::kIPv6FallbackTime);
    EXPECT_FALSE(connect_completers[2].is_connecting());
  } else {
    // For dual race, fresh_state_ has Fresh AAAA (v6) running. It started at
    // t=15ms. The slow timer for fresh_state_ will fire 300ms after t=15ms,
    // which is t=315ms.

    // First, fast forward exactly to t=300ms to ensure the fresh_state_ timer
    // didn't start prematurely at t=0ms.
    FastForwardBy(TcpConnectJob::kIPv6FallbackTime - kFreshAArrivalTime);
    EXPECT_FALSE(connect_completers[2].is_connecting());
    EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
    EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

    // Now fast forward the remaining 15ms to t=315ms.
    FastForwardBy(kFreshAAAAArrivalTime);

    // Since the Fresh A IPv4 address is part of the same ServiceEndpoint as
    // Fresh AAAA, the ipv4_connector is not blocked by current_endpoint_index
    // and starts immediately.
    EXPECT_TRUE(connect_completers[2].is_connecting());
    EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
    EXPECT_EQ(2u, connect_job_->GetFreshConnectorCountForTesting());
  }

  request->CallOnServiceEndpointRequestFinished(OK);

  if (!UseStaleConnectors()) {
    // If Fresh AAAA fails, primary_connector_ will immediately start Fresh A
    // because they are in the same ServiceEndpoint, avoiding the index
    // advancement block.
    connect_completers[1].Complete(ERR_CONNECTION_REFUSED);
    EXPECT_TRUE(connect_completers[2].is_connecting());

    // Now Stale A fails.
    connect_completers[0].Complete(ERR_CONNECTION_REFUSED);
  } else {
    EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
    EXPECT_EQ(2u, connect_job_->GetFreshConnectorCountForTesting());
    // For dual race, Fresh A is already running.
    connect_completers[1].Complete(ERR_CONNECTION_REFUSED);
    connect_completers[0].Complete(ERR_CONNECTION_REFUSED);
  }

  // Complete Fresh A.
  connect_completers[2].Complete(OK);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  WaitForSuccess(kIpV4Endpoint2, fresh_endpoint_merged,
                 {ConnectionAttempt(kIpV6Endpoint1, ERR_CONNECTION_REFUSED),
                  ConnectionAttempt(kIpV4Endpoint1, ERR_CONNECTION_REFUSED)});
}

TEST_P(TcpConnectJobOptimisticDnsTest, StaleAAAAThenDelayedFreshA) {
  const auto stale_endpoint = CreateServiceEndpoint({kIpV6Endpoint1});
  const auto fresh_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 2> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint1);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // t=0: Stale AAAA provided
  request->set_crypto_ready(true)
      .set_is_stale_while_refreshing(true)
      .set_endpoints({stale_endpoint})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_FALSE(connect_completers[1].is_connecting());
  if (GetParam()) {
    EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
    EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());
  }

  // t=500ms: Fresh A provided.
  constexpr base::TimeDelta kFreshAArrivalTime = base::Milliseconds(500);
  CHECK_GT(kFreshAArrivalTime, TcpConnectJob::kIPv6FallbackTime);
  FastForwardBy(kFreshAArrivalTime);

  // Since 500ms > 300ms, the slow_timer_ has already fired and created
  // `ipv4_connector_`. When Fresh A arrives, `ipv4_connector_` is free and
  // handles IPv4.
  request->set_is_stale_while_refreshing(false)
      .set_endpoints({fresh_endpoint})
      .CallOnServiceEndpointsUpdated();

  // It should immediately start connecting to Fresh A.
  EXPECT_TRUE(connect_completers[1].is_connecting());
  if (GetParam()) {
    EXPECT_EQ(2u, connect_job_->GetStaleConnectorCountForTesting());
    EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());
  }

  // Complete Fresh A.
  connect_completers[1].Complete(OK);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  WaitForSuccess(kIpV4Endpoint1, fresh_endpoint);
}

// Verify that enabling Optimistic DNS does not break standard Happy Eyeballs
// behavior when no stale records are available.
TEST_P(TcpConnectJobOptimisticDnsTest, NoStale_DelayedFreshAAAAThenA) {
  const auto fresh_endpoint_v6 = CreateServiceEndpoint({kIpV6Endpoint1});
  const auto fresh_endpoint_v4 = CreateServiceEndpoint({kIpV4Endpoint1});
  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 2> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint1);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // t=50ms: Fresh AAAA provided.
  constexpr base::TimeDelta kFreshAAAAArrivalTime = base::Milliseconds(50);
  CHECK_LT(kFreshAAAAArrivalTime, TcpConnectJob::kIPv6FallbackTime);
  FastForwardBy(kFreshAAAAArrivalTime);
  request->set_crypto_ready(true)
      .set_endpoints({fresh_endpoint_v6})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_FALSE(connect_completers[1].is_connecting());
  if (GetParam()) {
    EXPECT_EQ(0u, connect_job_->GetStaleConnectorCountForTesting());
    EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());
  }

  // t=100ms: Fresh A provided.
  constexpr base::TimeDelta kFreshAArrivalTime = base::Milliseconds(100);
  CHECK_LT(kFreshAArrivalTime, TcpConnectJob::kIPv6FallbackTime);
  FastForwardBy(kFreshAArrivalTime - kFreshAAAAArrivalTime);
  request->set_endpoints({fresh_endpoint_v6, fresh_endpoint_v4})
      .CallOnServiceEndpointsUpdated();

  // Because it has only been 50ms since the FIRST connection (Fresh AAAA)
  // started, we do not start connecting to Fresh A yet.
  EXPECT_FALSE(connect_completers[1].is_connecting());
  if (GetParam()) {
    EXPECT_EQ(0u, connect_job_->GetStaleConnectorCountForTesting());
    EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());
  }

  // t=350ms (300ms after Fresh AAAA started):
  FastForwardBy(TcpConnectJob::kIPv6FallbackTime -
                (kFreshAArrivalTime - kFreshAAAAArrivalTime));
  // ipv4_connector_ is created, but it is queued because primary_connector_ is
  // still occupied.
  EXPECT_FALSE(connect_completers[1].is_connecting());
  if (GetParam()) {
    EXPECT_EQ(0u, connect_job_->GetStaleConnectorCountForTesting());
    EXPECT_EQ(2u, connect_job_->GetFreshConnectorCountForTesting());
  }

  request->CallOnServiceEndpointRequestFinished(OK);

  // Fail Fresh AAAA.
  connect_completers[0].Complete(ERR_CONNECTION_REFUSED);
  // Now primary_connector_ can advance to fresh_endpoint_v4.
  EXPECT_TRUE(connect_completers[1].is_connecting());

  // Complete Fresh A.
  connect_completers[1].Complete(OK);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  WaitForSuccess(kIpV4Endpoint1, fresh_endpoint_v4,
                 {ConnectionAttempt(kIpV6Endpoint1, ERR_CONNECTION_REFUSED)});
}

TEST_P(TcpConnectJobOptimisticDnsTest, StaleAAAA_FailsBeforeFreshArrives) {
  const auto stale_endpoint = CreateServiceEndpoint({kIpV6Endpoint1});
  const auto fresh_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 2> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint1);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // t=0: Stale AAAA provided
  request->set_crypto_ready(true)
      .set_is_stale_while_refreshing(true)
      .set_endpoints({stale_endpoint})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(connect_completers[0].is_connecting());

  // t=100ms: Stale AAAA connection fails.
  constexpr base::TimeDelta kStaleFailureTime = base::Milliseconds(100);
  CHECK_LT(kStaleFailureTime, TcpConnectJob::kIPv6FallbackTime);
  FastForwardBy(kStaleFailureTime);
  connect_completers[0].Complete(ERR_CONNECTION_REFUSED);

  // No connection is established yet.
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());

  // t=200ms: Fresh A arrives.
  constexpr base::TimeDelta kFreshAArrivalTime = base::Milliseconds(200);
  CHECK_LT(kFreshAArrivalTime, TcpConnectJob::kIPv6FallbackTime);
  FastForwardBy(kFreshAArrivalTime - kStaleFailureTime);
  request->set_is_stale_while_refreshing(false)
      .set_endpoints({fresh_endpoint})
      .CallOnServiceEndpointsUpdated();

  // Since there are no active connections, it should immediately start
  // connecting to Fresh A.
  EXPECT_TRUE(connect_completers[1].is_connecting());

  connect_completers[1].Complete(OK);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  WaitForSuccess(kIpV4Endpoint1, fresh_endpoint,
                 {ConnectionAttempt(kIpV6Endpoint1, ERR_CONNECTION_REFUSED)});
}

TEST_P(TcpConnectJobOptimisticDnsTest, StalePromotedToFresh) {
  // Only run this when we have stale connectors to promote.
  if (!UseStaleConnectors()) {
    GTEST_SKIP()
        << "Connector promotion only applies when dual-race mode is enabled.";
  }

  const auto endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 1> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV4Endpoint1);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // t=0: Stale provided
  request->set_crypto_ready(true)
      .set_is_stale_while_refreshing(true)
      .set_endpoints({endpoint})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // t=50ms: Fresh endpoint (same IP) arrives.
  constexpr base::TimeDelta kFreshArrivalTime = base::Milliseconds(50);
  CHECK_LT(kFreshArrivalTime, TcpConnectJob::kIPv6FallbackTime);
  FastForwardBy(kFreshArrivalTime);
  request->set_is_stale_while_refreshing(false)
      .set_endpoints({endpoint})
      .CallOnServiceEndpointsUpdated();

  // The running stale attempt should have been promoted to fresh.
  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_EQ(0u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // Fast forward past the fallback time (e.g., 400ms) to prove there's no
  // waiting for the 300ms stale timer or any secondary fallback if it's
  // already promoted.
  constexpr base::TimeDelta kFutureTime = base::Milliseconds(400);
  CHECK_GT(kFutureTime, TcpConnectJob::kIPv6FallbackTime);
  FastForwardBy(kFutureTime - kFreshArrivalTime);

  // The connection should still be running as a fresh attempt.
  EXPECT_TRUE(connect_completers[0].is_connecting());

  request->CallOnServiceEndpointRequestFinished(OK);
  connect_completers[0].Complete(OK);

  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  WaitForSuccess(kIpV4Endpoint1, endpoint);
}

TEST_P(TcpConnectJobOptimisticDnsTest, StalePromotedToFresh_AfterFreshFails) {
  // Only run this when we have stale connectors to promote.
  if (!UseStaleConnectors()) {
    GTEST_SKIP()
        << "Connector promotion only applies when dual-race mode is enabled.";
  }

  const auto stale_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  const auto fresh_endpoint1 = CreateServiceEndpoint({kIpV4Endpoint2});
  const auto fresh_endpoint2 =
      CreateServiceEndpoint({kIpV4Endpoint2, kIpV4Endpoint1});
  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 2> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV4Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint2);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // t=0: Stale provided with endpoint1.
  request->set_crypto_ready(true)
      .set_is_stale_while_refreshing(true)
      .set_endpoints({stale_endpoint})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // t=50ms: Fresh endpoint (endpoint2) arrives.
  constexpr base::TimeDelta kFreshArrivalTime = base::Milliseconds(50);
  FastForwardBy(kFreshArrivalTime);
  request->set_is_stale_while_refreshing(false)
      .set_endpoints({fresh_endpoint1})
      .CallOnServiceEndpointsUpdated();

  // Fresh connector starts.
  EXPECT_TRUE(connect_completers[1].is_connecting());
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // Fresh connector fails.
  connect_completers[1].Complete(ERR_CONNECTION_REFUSED);
  // Stale connector is still running, job should not be done yet.
  EXPECT_FALSE(test_delegate_->has_result());
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // t=100ms: Fresh endpoint updates to include endpoint1 as well.
  constexpr base::TimeDelta kFreshUpdateArrivalTime = base::Milliseconds(50);
  FastForwardBy(kFreshUpdateArrivalTime);
  request->set_endpoints({fresh_endpoint2}).CallOnServiceEndpointsUpdated();

  // The running stale attempt should have been promoted to fresh because it
  // matches endpoint1.
  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_EQ(0u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // Complete the DNS request to prove fresh state can finish normally.
  request->CallOnServiceEndpointRequestFinished(OK);
  connect_completers[0].Complete(OK);

  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  WaitForSuccess(kIpV4Endpoint1, fresh_endpoint2,
                 {ConnectionAttempt(kIpV4Endpoint2, ERR_CONNECTION_REFUSED)});
}

TEST_P(TcpConnectJobOptimisticDnsTest,
       StalePromotedToFresh_FailsAndFallsBackToNextFresh) {
  // Only run this when we have stale connectors to promote.
  if (!UseStaleConnectors()) {
    GTEST_SKIP()
        << "Connector promotion only applies when dual-race mode is enabled.";
  }

  const auto stale_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  const auto fresh_endpoint =
      CreateServiceEndpoint({kIpV4Endpoint1, kIpV4Endpoint2});
  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 2> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV4Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint2);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // t=0: Stale provided
  request->set_crypto_ready(true)
      .set_is_stale_while_refreshing(true)
      .set_endpoints({stale_endpoint})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // t=50ms: Fresh endpoint (same IP, plus another) arrives.
  constexpr base::TimeDelta kFreshArrivalTime = base::Milliseconds(50);
  CHECK_LT(kFreshArrivalTime, TcpConnectJob::kIPv6FallbackTime);
  FastForwardBy(kFreshArrivalTime);
  request->set_is_stale_while_refreshing(false)
      .set_endpoints({fresh_endpoint})
      .CallOnServiceEndpointsUpdated();

  // The running stale attempt should have been promoted to fresh.
  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_EQ(0u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // The promoted connection fails.
  connect_completers[0].Complete(ERR_CONNECTION_REFUSED);

  // The connection should not prematurely complete, but instead start
  // connecting to the next available fresh endpoint.
  EXPECT_FALSE(connect_job_->HasEstablishedConnection());
  EXPECT_TRUE(connect_completers[1].is_connecting());
  EXPECT_EQ(0u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  request->CallOnServiceEndpointRequestFinished(OK);
  connect_completers[1].Complete(OK);

  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  WaitForSuccess(kIpV4Endpoint2, fresh_endpoint,
                 {ConnectionAttempt(kIpV4Endpoint1, ERR_CONNECTION_REFUSED)});
}

TEST_P(TcpConnectJobOptimisticDnsTest,
       StaleIpv4CompletesAfterStalePrimaryFailsAndFreshArrives) {
  // Only run this when we have stale connectors to promote.
  if (!UseStaleConnectors()) {
    GTEST_SKIP()
        << "Connector promotion only applies when dual-race mode is enabled.";
  }

  const auto stale_endpoint =
      CreateServiceEndpoint({kIpV6Endpoint1, kIpV4Endpoint1});
  const auto fresh_endpoint =
      CreateServiceEndpoint({kIpV6Endpoint2, kIpV4Endpoint2});
  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 3> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint1);
  AddConnect(MockConnect(&connect_completers[2]), kIpV6Endpoint2);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // t=0: Stale provided with v6 and v4.
  request->set_crypto_ready(true)
      .set_is_stale_while_refreshing(true)
      .set_endpoints({stale_endpoint})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  // Primary stale connector starts (IPv6).
  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // t=300ms: Fast-forward to trigger Happy Eyeballs for stale IPv4.
  FastForwardBy(TcpConnectJob::kIPv6FallbackTime);
  EXPECT_TRUE(connect_completers[1].is_connecting());
  EXPECT_EQ(2u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // The primary stale connector fails.
  connect_completers[0].Complete(ERR_CONNECTION_REFUSED);
  EXPECT_EQ(2u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // Fresh endpoints arrive, but they do not match the stale endpoints,
  // so no promotion happens.
  request->set_is_stale_while_refreshing(false)
      .set_endpoints({fresh_endpoint})
      .CallOnServiceEndpointsUpdated();

  // Fresh primary connector starts (IPv6).
  EXPECT_TRUE(connect_completers[2].is_connecting());
  EXPECT_EQ(2u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // The stale IPv4 connector completes successfully.
  // Ensure that the stale state does not hang, since the primary connector is
  // already done.
  connect_completers[1].Complete(OK);

  // The job should complete successfully using the stale IPv4 endpoint.
  EXPECT_TRUE(test_delegate_->has_result());
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  WaitForSuccess(kIpV4Endpoint1, stale_endpoint,
                 {ConnectionAttempt(kIpV6Endpoint1, ERR_CONNECTION_REFUSED)});
}

TEST_P(TcpConnectJobOptimisticDnsTest,
       StalePromotedToFresh_StaggeredResults_FreshBusy) {
  // Only run this when we have stale connectors to promote.
  if (!UseStaleConnectors()) {
    GTEST_SKIP()
        << "Connector promotion only applies when dual-race mode is enabled.";
  }

  const auto stale_endpoint = CreateServiceEndpoint({kIpV6Endpoint1});
  const auto fresh_endpoint_batch1 = CreateServiceEndpoint({kIpV6Endpoint2});
  const auto fresh_endpoint_batch2 =
      CreateServiceEndpoint({kIpV6Endpoint2, kIpV6Endpoint1});

  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 2> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV6Endpoint2);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // t=0: Stale provided with v6.
  request->set_crypto_ready(true)
      .set_is_stale_while_refreshing(true)
      .set_endpoints({stale_endpoint})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  // Primary stale connector starts (IPv6 Endpoint 1).
  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // Fresh batch 1 arrives, which doesn't match with the stale endpoint.
  request->set_is_stale_while_refreshing(false)
      .set_endpoints({fresh_endpoint_batch1})
      .CallOnServiceEndpointsUpdated();

  // Fresh primary connector starts (IPv6 Endpoint 2).
  EXPECT_TRUE(connect_completers[1].is_connecting());
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // Fresh batch 2 arrives. Matches stale endpoint, but fresh_primary is busy
  // connecting to Endpoint 2.
  request->set_endpoints({fresh_endpoint_batch2})
      .CallOnServiceEndpointsUpdated();
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // If stale was incorrectly promoted, the fresh_primary (connecting to
  // Endpoint 2) would be destroyed. We verify it's still alive by completing it
  // successfully.
  connect_completers[1].Complete(OK);

  EXPECT_TRUE(test_delegate_->has_result());
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  WaitForSuccess(kIpV6Endpoint2, fresh_endpoint_batch2, {});
}

TEST_P(TcpConnectJobOptimisticDnsTest,
       StalePromotedToFresh_StaggeredResults_FreshIdle) {
  // Only run this when we have stale connectors to promote.
  if (!UseStaleConnectors()) {
    GTEST_SKIP()
        << "Connector promotion only applies when dual-race mode is enabled.";
  }

  const auto stale_endpoint = CreateServiceEndpoint({kIpV6Endpoint1});
  const auto fresh_endpoint_batch1 = CreateServiceEndpoint({kIpV6Endpoint2});
  const auto fresh_endpoint_batch2 =
      CreateServiceEndpoint({kIpV6Endpoint1, kIpV6Endpoint3});

  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 3> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV6Endpoint2);
  AddConnect(MockConnect(&connect_completers[2]), kIpV6Endpoint3);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // t=0: Stale provided with v6.
  request->set_crypto_ready(true)
      .set_is_stale_while_refreshing(true)
      .set_endpoints({stale_endpoint})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  // Primary stale connector starts (IPv6 Endpoint 1).
  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // Fresh batch 1 arrives. Does NOT match.
  request->set_is_stale_while_refreshing(false)
      .set_endpoints({fresh_endpoint_batch1})
      .CallOnServiceEndpointsUpdated();

  // Fresh primary connector starts (IPv6 Endpoint 2).
  EXPECT_TRUE(connect_completers[1].is_connecting());
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // Fresh primary fails Endpoint 2.
  connect_completers[1].Complete(ERR_CONNECTION_REFUSED);
  // It is now idle, waiting for endpoints.
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // Fresh batch 2 arrives. Matches stale endpoint.
  request->set_endpoints({fresh_endpoint_batch2})
      .CallOnServiceEndpointsUpdated();

  // If stale was correctly promoted, fresh_primary is now Endpoint 1.
  // Endpoint 1 is still connecting, so fresh_primary is busy.
  // So Endpoint 3 should not be connecting.
  EXPECT_FALSE(connect_completers[2].is_connecting());
  EXPECT_EQ(0u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // Now fail the promoted connector (Endpoint 1).
  connect_completers[0].Complete(ERR_CONNECTION_REFUSED);

  // Now Endpoint 3 should start connecting, because the slot opened up.
  EXPECT_TRUE(connect_completers[2].is_connecting());

  // Complete Endpoint 3 successfully.
  connect_completers[2].Complete(OK);

  EXPECT_TRUE(test_delegate_->has_result());
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  WaitForSuccess(kIpV6Endpoint3, fresh_endpoint_batch2,
                 {ConnectionAttempt(kIpV6Endpoint2, ERR_CONNECTION_REFUSED),
                  ConnectionAttempt(kIpV6Endpoint1, ERR_CONNECTION_REFUSED)});
}

TEST_P(TcpConnectJobOptimisticDnsTest,
       StalePromotedToFresh_StaleFails_FreshProceeds) {
  // Only run this when we have stale connectors to promote.
  if (!UseStaleConnectors()) {
    GTEST_SKIP()
        << "Connector promotion only applies when dual-race mode is enabled.";
  }

  const auto stale_endpoint = CreateServiceEndpoint({kIpV6Endpoint1});
  const auto fresh_endpoint =
      CreateServiceEndpoint({kIpV6Endpoint1, kIpV6Endpoint2});

  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 2> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV6Endpoint2);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // t=0: Stale provided with v6.
  request->set_crypto_ready(true)
      .set_is_stale_while_refreshing(true)
      .set_endpoints({stale_endpoint})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // Fresh arrives, matches stale endpoint, and is promoted.
  request->set_is_stale_while_refreshing(false)
      .set_endpoints({fresh_endpoint})
      .CallOnServiceEndpointsUpdated();
  EXPECT_EQ(0u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // Fail the promoted connector.
  connect_completers[0].Complete(ERR_CONNECTION_REFUSED);

  // Fresh should seamlessly proceed to the next IP.
  EXPECT_TRUE(connect_completers[1].is_connecting());
  EXPECT_EQ(0u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  connect_completers[1].Complete(OK);

  EXPECT_TRUE(test_delegate_->has_result());
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  WaitForSuccess(kIpV6Endpoint2, fresh_endpoint,
                 {ConnectionAttempt(kIpV6Endpoint1, ERR_CONNECTION_REFUSED)});
}

TEST_P(TcpConnectJobOptimisticDnsTest,
       StalePromotedToFresh_TimerAccountsForElapsedTime) {
  // Only run this when we have stale connectors to promote.
  if (!UseStaleConnectors()) {
    GTEST_SKIP()
        << "Connector promotion only applies when dual-race mode is enabled.";
  }

  const auto stale_endpoint =
      CreateServiceEndpoint({kIpV6Endpoint1, kIpV4Endpoint1});
  const auto fresh_endpoint =
      CreateServiceEndpoint({kIpV6Endpoint1, kIpV4Endpoint1});

  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 2> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint1);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // t=0: Stale provided.
  request->set_crypto_ready(true)
      .set_is_stale_while_refreshing(true)
      .set_endpoints({stale_endpoint})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_FALSE(connect_completers[1].is_connecting());

  // Fast forward 200ms. The timer for IPv4 fallback needs 300ms, so it
  // shouldn't fire.
  FastForwardBy(base::Milliseconds(200));
  EXPECT_FALSE(connect_completers[1].is_connecting());

  // Fresh arrives, matches stale endpoint, and is promoted.
  request->set_is_stale_while_refreshing(false)
      .set_endpoints({fresh_endpoint})
      .CallOnServiceEndpointsUpdated();

  // The fresh state's timer should have been started with 100ms remaining,
  // since 200ms of the 300ms fallback time has already elapsed.
  FastForwardBy(base::Milliseconds(99));
  EXPECT_FALSE(connect_completers[1].is_connecting());

  // 1ms later, the 300ms total elapsed time is reached, and the IPv4 fallback
  // starts.
  FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(connect_completers[1].is_connecting());

  request->CallOnServiceEndpointRequestFinished(OK);
  connect_completers[1].Complete(OK);

  EXPECT_TRUE(test_delegate_->has_result());
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  WaitForSuccess(kIpV4Endpoint1, fresh_endpoint);
}

TEST_P(TcpConnectJobOptimisticDnsTest, StaleAndFreshRacingIndependently) {
  // Only run this when we have dual race support.
  if (!UseStaleConnectors()) {
    GTEST_SKIP()
        << "Connector promotion only applies when dual-race mode is enabled.";
  }

  const auto stale_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  const auto fresh_endpoint = CreateServiceEndpoint({kIpV4Endpoint2});
  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 2> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV4Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint2);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // t=0: Stale A provided
  request->set_crypto_ready(true)
      .set_is_stale_while_refreshing(true)
      .set_endpoints({stale_endpoint})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_FALSE(connect_completers[1].is_connecting());
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // t=50ms: Fresh B provided (different IP).
  constexpr base::TimeDelta kFreshBArrivalTime = base::Milliseconds(50);
  CHECK_LT(kFreshBArrivalTime, TcpConnectJob::kIPv6FallbackTime);
  FastForwardBy(kFreshBArrivalTime);
  request->set_is_stale_while_refreshing(false)
      .set_endpoints({fresh_endpoint})
      .CallOnServiceEndpointsUpdated();

  // Fresh B should start immediately, racing independently with Stale A.
  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_TRUE(connect_completers[1].is_connecting());
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  request->CallOnServiceEndpointRequestFinished(OK);
  connect_completers[1].Complete(OK);

  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  WaitForSuccess(kIpV4Endpoint2, fresh_endpoint);
}

TEST_P(TcpConnectJobOptimisticDnsTest, UnstartedStaleCleared) {
  // Only run this when we have dual race support.
  if (!UseStaleConnectors()) {
    GTEST_SKIP()
        << "Connector promotion only applies when dual-race mode is enabled.";
  }

  const auto stale_endpoint_1 = CreateServiceEndpoint({kIpV4Endpoint1});
  const auto stale_endpoint_2 = CreateServiceEndpoint({kIpV4Endpoint2});
  const auto fresh_endpoint = CreateServiceEndpoint({kIpV4Endpoint3});
  auto request = host_resolver_.AddFakeRequest();

  // Two connect completers, since the second stale endpoint should never start.
  std::array<MockConnectCompleter, 2> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV4Endpoint1);
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint3);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // t=0: Both Stale endpoints provided
  request->set_crypto_ready(true)
      .set_is_stale_while_refreshing(true)
      .set_endpoints({stale_endpoint_1, stale_endpoint_2})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  // Only the first one starts immediately.
  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // t=50ms: Fresh endpoint arrives. It does not include the stale endpoints.
  constexpr base::TimeDelta kFreshArrivalTime = base::Milliseconds(50);
  CHECK_LT(kFreshArrivalTime, TcpConnectJob::kIPv6FallbackTime);
  FastForwardBy(kFreshArrivalTime);
  request->set_is_stale_while_refreshing(false)
      .set_endpoints({fresh_endpoint})
      .CallOnServiceEndpointsUpdated();

  // Fresh endpoint should start immediately alongside the running stale
  // endpoint.
  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_TRUE(connect_completers[1].is_connecting());
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // Fast forward past the 300ms timer point (e.g. 400ms).
  // The unstarted Stale IP 2 should not start because it was cleared.
  constexpr base::TimeDelta kFutureTime = base::Milliseconds(400);
  CHECK_GT(kFutureTime, TcpConnectJob::kIPv6FallbackTime);
  FastForwardBy(kFutureTime - kFreshArrivalTime);
  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_TRUE(connect_completers[1].is_connecting());
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(2u, connect_job_->GetFreshConnectorCountForTesting());

  request->CallOnServiceEndpointRequestFinished(OK);
  connect_completers[1].Complete(OK);

  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  WaitForSuccess(kIpV4Endpoint3, fresh_endpoint);
}

TEST_P(TcpConnectJobOptimisticDnsTest,
       StaleWhileRefreshing_FreshStateDoesNotAdvance) {
  // This test only applies to dual-race mode.
  if (!UseStaleConnectors()) {
    GTEST_SKIP()
        << "Connector promotion only applies when dual-race mode is enabled.";
  }

  const auto stale_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 1> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV4Endpoint1);

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // Stale endpoint is provided.
  request->set_crypto_ready(true)
      .set_is_stale_while_refreshing(true)
      .set_endpoints({stale_endpoint})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  // The crucial check for step 2: fresh_state_ should not be advanced.
  // It is created in the constructor, so the count is 1, but it hasn't started.
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // Triggering another event (e.g. crypto ready change) should still not
  // advance fresh_state_ while IsStaleWhileRefreshing() is true.
  request->set_crypto_ready(false).CallOnServiceEndpointsUpdated();
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // Complete the stale connection.
  request->CallOnServiceEndpointRequestFinished(OK);
  connect_completers[0].Complete(OK);

  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  WaitForSuccess(kIpV4Endpoint1, stale_endpoint);
}

TEST_P(TcpConnectJobOptimisticDnsTest, FreshDone_DnsCompletes_DoesNotCrash) {
  // This test only applies to dual-race mode.
  if (!UseStaleConnectors()) {
    GTEST_SKIP()
        << "Connector promotion only applies when dual-race mode is enabled.";
  }

  const auto stale_endpoint = CreateServiceEndpoint({kIpV6Endpoint1});
  const auto fresh_endpoint = CreateServiceEndpoint({kIpV4Endpoint1});
  auto request = host_resolver_.AddFakeRequest();

  std::array<MockConnectCompleter, 2> connect_completers;
  AddConnect(MockConnect(&connect_completers[0]), kIpV6Endpoint1);  // Stale
  AddConnect(MockConnect(&connect_completers[1]), kIpV4Endpoint1);  // Fresh

  EXPECT_THAT(InitAndStart(), IsError(ERR_IO_PENDING));

  // Stale arrives.
  request->set_crypto_ready(true)
      .set_is_stale_while_refreshing(true)
      .set_endpoints({stale_endpoint})
      .set_aliases(kDnsAliases)
      .CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(connect_completers[0].is_connecting());
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());

  // Fresh arrives.
  request->set_is_stale_while_refreshing(false)
      .set_endpoints({fresh_endpoint})
      .CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(connect_completers[1].is_connecting());
  EXPECT_EQ(1u, connect_job_->GetStaleConnectorCountForTesting());
  EXPECT_EQ(1u, connect_job_->GetFreshConnectorCountForTesting());

  // Fresh fails immediately, exhausting all its endpoints and becoming "done".
  connect_completers[1].Complete(ERR_CONNECTION_REFUSED);
  // Job is not done because stale is still running.
  EXPECT_FALSE(test_delegate_->has_result());

  // Now the DNS request finishes. This triggers
  // DoTryAdvanceWaitingConnectors(). It shouldn't crash trying to advance the
  // completed fresh_state_!
  request->CallOnServiceEndpointRequestFinished(OK);

  // Still not done, stale is running.
  EXPECT_FALSE(test_delegate_->has_result());

  // Stale completes successfully.
  connect_completers[0].Complete(OK);
  EXPECT_TRUE(connect_job_->HasEstablishedConnection());
  WaitForSuccess(kIpV6Endpoint1, stale_endpoint,
                 {ConnectionAttempt(kIpV4Endpoint1, ERR_CONNECTION_REFUSED)});
}

}  // namespace
}  // namespace net
