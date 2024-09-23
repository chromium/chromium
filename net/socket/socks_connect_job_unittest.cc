// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socks_connect_job.h"

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/log/net_log.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/connect_job_test_util.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/transport_client_socket_pool_test_util.h"
#include "net/socket/transport_connect_job.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

const char kProxyHostName[] = "proxy.test";
const int kProxyPort = 4321;

constexpr base::TimeDelta kTinyTime = base::Microseconds(1);

class SOCKSConnectJobTest : public testing::Test, public WithTaskEnvironment {
 public:
  enum class SOCKSVersion {
    V4,
    V5,
  };

  SOCKSConnectJobTest()
      : WithTaskEnvironment(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        common_connect_job_params_(
            &client_socket_factory_,
            &host_resolver_,
            /*http_auth_cache=*/nullptr,
            /*http_auth_handler_factory=*/nullptr,
            /*spdy_session_pool=*/nullptr,
            /*quic_supported_versions=*/nullptr,
            /*quic_session_pool=*/nullptr,
            /*proxy_delegate=*/nullptr,
            &http_user_agent_settings_,
            /*ssl_client_context=*/nullptr,
            /*socket_performance_watcher_factory=*/nullptr,
            /*network_quality_estimator=*/nullptr,
            NetLog::Get(),
            /*websocket_endpoint_lock_manager=*/nullptr,
            /*http_server_properties=*/nullptr,
            /*alpn_protos=*/nullptr,
            /*application_settings=*/nullptr,
            /*ignore_certificate_errors=*/nullptr,
            /*early_data_enabled=*/nullptr) {}

  ~SOCKSConnectJobTest() override = default;

  static scoped_refptr<SOCKSSocketParams> CreateSOCKSParams(
      SOCKSVersion socks_version,
      SecureDnsPolicy secure_dns_policy = SecureDnsPolicy::kAllow) {
    return base::MakeRefCounted<SOCKSSocketParams>(
        ConnectJobParams(base::MakeRefCounted<TransportSocketParams>(
            HostPortPair(kProxyHostName, kProxyPort), NetworkAnonymizationKey(),
            secure_dns_policy, OnHostResolutionCallback(),
            /*supported_alpns=*/base::flat_set<std::string>())),
        socks_version == SOCKSVersion::V5,
        socks_version == SOCKSVersion::V4
            ? HostPortPair(kSOCKS4TestHost, kSOCKS4TestPort)
            : HostPortPair(kSOCKS5TestHost, kSOCKS5TestPort),
        NetworkAnonymizationKey(), TRAFFIC_ANNOTATION_FOR_TESTS);
  }

 protected:
  MockHostResolver host_resolver_{/*default_result=*/MockHostResolverBase::
                                      RuleResolver::GetLocalhostResult()};
  MockTaggingClientSocketFactory client_socket_factory_;
  const StaticHttpUserAgentSettings http_user_agent_settings_ = {"*",
                                                                 "test-ua"};
  const CommonConnectJobParams common_connect_job_params_;
};

TEST_F(SOCKSConnectJobTest, HostResolutionFailure) {
  host_resolver_.rules()->AddSimulatedTimeoutFailure(kProxyHostName);

  for (bool failure_synchronous : {false, true}) {
    host_resolver_.set_synchronous_mode(failure_synchronous);
    TestConnectJobDelegate test_delegate;
    SOCKSConnectJob socks_connect_job(DEFAULT_PRIORITY, SocketTag(),
                                      &common_connect_job_params_,
                                      CreateSOCKSParams(SOCKSVersion::V5),
                                      &test_delegate, nullptr /* net_log */);
    test_delegate.StartJobExpectingResult(
        &socks_connect_job, ERR_PROXY_CONNECTION_FAILED, failure_synchronous);
    EXPECT_THAT(socks_connect_job.GetResolveErrorInfo().error,
                test::IsError(ERR_DNS_TIMED_OUT));
  }
}

TEST_F(SOCKSConnectJobTest, HostResolutionFailureSOCKS4Endpoint) {
  const char hostname[] = "google.com";
  host_resolver_.rules()->AddSimulatedTimeoutFailure(hostname);

  for (bool failure_synchronous : {false, true}) {
    host_resolver_.set_synchronous_mode(failure_synchronous);

    SequencedSocketData sequenced_socket_data{base::span<MockRead>(),
                                              base::span<MockWrite>()};
    sequenced_socket_data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
    client_socket_factory_.AddSocketDataProvider(&sequenced_socket_data);

    scoped_refptr<SOCKSSocketParams> socket_params =
        base::MakeRefCounted<SOCKSSocketParams>(
            ConnectJobParams(base::MakeRefCounted<TransportSocketParams>(
                HostPortPair(kProxyHostName, kProxyPort),
                NetworkAnonymizationKey(), SecureDnsPolicy::kAllow,
                OnHostResolutionCallback(),
                /*supported_alpns=*/base::flat_set<std::string>())),
            false /* socks_v5 */, HostPortPair(hostname, kSOCKS4TestPort),
            NetworkAnonymizationKey(), TRAFFIC_ANNOTATION_FOR_TESTS);

    TestConnectJobDelegate test_delegate;
    SOCKSConnectJob socks_connect_job(
        DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
        socket_params, &test_delegate, nullptr /* net_log */);
    test_delegate.StartJobExpectingResult(
        &socks_connect_job, ERR_NAME_NOT_RESOLVED, failure_synchronous);
    EXPECT_THAT(socks_connect_job.GetResolveErrorInfo().error,
                test::IsError(ERR_DNS_TIMED_OUT));
  }
}

TEST_F(SOCKSConnectJobTest, HandshakeError) {
  for (bool host_resolution_synchronous : {false, true}) {
    for (bool write_failure_synchronous : {false, true}) {
      host_resolver_.set_synchronous_mode(host_resolution_synchronous);

      // No need to distinguish which part of the handshake fails. Those details
      // are all handled at the StreamSocket layer, not the SOCKSConnectJob.
      MockWrite writes[] = {
          MockWrite(write_failure_synchronous ? SYNCHRONOUS : ASYNC,
                    ERR_UNEXPECTED, 0),
      };
      SequencedSocketData sequenced_socket_data(base::span<MockRead>(), writes);
      // Host resolution is used to switch between sync and async connection
      // behavior. The SOCKS layer can't distinguish between sync and async host
      // resolution vs sync and async connection establishment, so just always
      // make connection establishment synchroonous.
      sequenced_socket_data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
      client_socket_factory_.AddSocketDataProvider(&sequenced_socket_data);

      TestConnectJobDelegate test_delegate;
      SOCKSConnectJob socks_connect_job(DEFAULT_PRIORITY, SocketTag(),
                                        &common_connect_job_params_,
                                        CreateSOCKSParams(SOCKSVersion::V5),
                                        &test_delegate, nullptr /* net_log */);
      test_delegate.StartJobExpectingResult(
          &socks_connect_job, ERR_UNEXPECTED,
          host_resolution_synchronous && write_failure_synchronous);
    }
  }
}

TEST_F(SOCKSConnectJobTest, SOCKS4) {
  for (bool host_resolution_synchronous : {false, true}) {
    for (bool read_and_writes_synchronous : {true}) {
      host_resolver_.set_synchronous_mode(host_resolution_synchronous);

      MockWrite writes[] = {
          MockWrite(SYNCHRONOUS, kSOCKS4OkRequestLocalHostPort80,
                    kSOCKS4OkRequestLocalHostPort80Length, 0),
      };

      MockRead reads[] = {
          MockRead(SYNCHRONOUS, kSOCKS4OkReply, kSOCKS4OkReplyLength, 1),
      };

      SequencedSocketData sequenced_socket_data(reads, writes);
      // Host resolution is used to switch between sync and async connection
      // behavior. The SOCKS layer can't distinguish between sync and async host
      // resolution vs sync and async connection establishment, so just always
      // make connection establishment synchroonous.
      sequenced_socket_data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
      client_socket_factory_.AddSocketDataProvider(&sequenced_socket_data);

      TestConnectJobDelegate test_delegate;
      SOCKSConnectJob socks_connect_job(DEFAULT_PRIORITY, SocketTag(),
                                        &common_connect_job_params_,
                                        CreateSOCKSParams(SOCKSVersion::V4),
                                        &test_delegate, nullptr /* net_log */);
      test_delegate.StartJobExpectingResult(
          &socks_connect_job, OK,
          host_resolution_synchronous && read_and_writes_synchronous);

      // Proxies should not set any DNS aliases.
      EXPECT_TRUE(test_delegate.socket()->GetDnsAliases().empty());
    }
  }
}

TEST_F(SOCKSConnectJobTest, SOCKS5) {
  for (bool host_resolution_synchronous : {false, true}) {
    for (bool read_and_writes_synchronous : {true}) {
      host_resolver_.set_synchronous_mode(host_resolution_synchronous);

      MockWrite writes[] = {
          MockWrite(SYNCHRONOUS, kSOCKS5GreetRequest, kSOCKS5GreetRequestLength,
                    0),
          MockWrite(SYNCHRONOUS, kSOCKS5OkRequest, kSOCKS5OkRequestLength, 2),
      };

      MockRead reads[] = {
          MockRead(SYNCHRONOUS, kSOCKS5GreetResponse,
                   kSOCKS5GreetResponseLength, 1),
          MockRead(SYNCHRONOUS, kSOCKS5OkResponse, kSOCKS5OkResponseLength, 3),
      };

      SequencedSocketData sequenced_socket_data(reads, writes);
      // Host resolution is used to switch between sync and async connection
      // behavior. The SOCKS layer can't distinguish between sync and async host
      // resolution vs sync and async connection establishment, so just always
      // make connection establishment synchroonous.
      sequenced_socket_data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
      client_socket_factory_.AddSocketDataProvider(&sequenced_socket_data);

      TestConnectJobDelegate test_delegate;
      SOCKSConnectJob socks_connect_job(DEFAULT_PRIORITY, SocketTag(),
                                        &common_connect_job_params_,
                                        CreateSOCKSParams(SOCKSVersion::V5),
                                        &test_delegate, nullptr /* net_log */);
      test_delegate.StartJobExpectingResult(
          &socks_connect_job, OK,
          host_resolution_synchronous && read_and_writes_synchronous);

      // Proxies should not set any DNS aliases.
      EXPECT_TRUE(test_delegate.socket()->GetDnsAliases().empty());
    }
  }
}

TEST_F(SOCKSConnectJobTest, HasEstablishedConnection) {
  host_resolver_.set_ondemand_mode(true);
  MockWrite writes[] = {
      MockWrite(ASYNC, kSOCKS4OkRequestLocalHostPort80,
                kSOCKS4OkRequestLocalHostPort80Length, 0),
  };

  MockRead reads[] = {
      MockRead(ASYNC, ERR_IO_PENDING, 1),
      MockRead(ASYNC, kSOCKS4OkReply, kSOCKS4OkReplyLength, 2),
  };

  SequencedSocketData sequenced_socket_data(reads, writes);
  sequenced_socket_data.set_connect_data(MockConnect(ASYNC, OK));
  client_socket_factory_.AddSocketDataProvider(&sequenced_socket_data);

  TestConnectJobDelegate test_delegate;
  SOCKSConnectJob socks_connect_job(DEFAULT_PRIORITY, SocketTag(),
                                    &common_connect_job_params_,
                                    CreateSOCKSParams(SOCKSVersion::V4),
                                    &test_delegate, nullptr /* net_log */);
  socks_connect_job.Connect();
  EXPECT_EQ(LOAD_STATE_RESOLVING_HOST, socks_connect_job.GetLoadState());
  EXPECT_FALSE(socks_connect_job.HasEstablishedConnection());

  host_resolver_.ResolveNow(1);
  EXPECT_EQ(LOAD_STATE_CONNECTING, socks_connect_job.GetLoadState());
  EXPECT_FALSE(socks_connect_job.HasEstablishedConnection());

  sequenced_socket_data.RunUntilPaused();
  // "LOAD_STATE_CONNECTING" is also returned when negotiating a SOCKS
  // connection.
  EXPECT_EQ(LOAD_STATE_CONNECTING, socks_connect_job.GetLoadState());
  EXPECT_TRUE(socks_connect_job.HasEstablishedConnection());
  EXPECT_FALSE(test_delegate.has_result());

  sequenced_socket_data.Resume();
  EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());
  EXPECT_TRUE(test_delegate.has_result());
}

// Check that TransportConnectJob's timeout is respected for the nested
// TransportConnectJob.
TEST_F(SOCKSConnectJobTest, TimeoutDuringDnsResolution) {
  // Set HostResolver to hang.
  host_resolver_.set_ondemand_mode(true);

  TestConnectJobDelegate test_delegate;
  SOCKSConnectJob socks_connect_job(DEFAULT_PRIORITY, SocketTag(),
                                    &common_connect_job_params_,
                                    CreateSOCKSParams(SOCKSVersion::V5),
                                    &test_delegate, nullptr /* net_log */);
  socks_connect_job.Connect();

  // Just before the TransportConnectJob's timeout, nothing should have
  // happened.
  FastForwardBy(TransportConnectJob::ConnectionTimeout() - kTinyTime);
  EXPECT_TRUE(host_resolver_.has_pending_requests());
  EXPECT_FALSE(test_delegate.has_result());

  // Wait for exactly the TransportConnectJob's timeout to have passed. The Job
  // should time out.
  FastForwardBy(kTinyTime);
  EXPECT_TRUE(test_delegate.has_result());
  EXPECT_THAT(test_delegate.WaitForResult(),
              test::IsError(ERR_PROXY_CONNECTION_FAILED));
}

// Check that SOCKSConnectJob's timeout is respected for the handshake phase.
TEST_F(SOCKSConnectJobTest, TimeoutDuringHandshake) {
  host_resolver_.set_ondemand_mode(true);

  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 0),
  };

  SequencedSocketData sequenced_socket_data(base::span<MockRead>(), writes);
  sequenced_socket_data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  client_socket_factory_.AddSocketDataProvider(&sequenced_socket_data);

  TestConnectJobDelegate test_delegate;
  SOCKSConnectJob socks_connect_job(DEFAULT_PRIORITY, SocketTag(),
                                    &common_connect_job_params_,
                                    CreateSOCKSParams(SOCKSVersion::V5),
                                    &test_delegate, nullptr /* net_log */);
  socks_connect_job.Connect();

  // Just before the TransportConnectJob's timeout, nothing should have
  // happened.
  FastForwardBy(TransportConnectJob::ConnectionTimeout() - kTinyTime);
  EXPECT_FALSE(test_delegate.has_result());
  EXPECT_TRUE(host_resolver_.has_pending_requests());

  // DNS resolution completes, and the socket connects.  The request should not
  // time out, even after the TransportConnectJob's timeout passes. The
  // SOCKSConnectJob's handshake timer should also be started.
  host_resolver_.ResolveAllPending();

  // Waiting until just before the SOCKS handshake times out. There should cause
  // no observable change in the SOCKSConnectJob's status.
  FastForwardBy(SOCKSConnectJob::HandshakeTimeoutForTesting() - kTinyTime);
  EXPECT_FALSE(test_delegate.has_result());

  // Wait for exactly the SOCKSConnectJob's handshake timeout has fully elapsed.
  // The Job should time out.
  FastForwardBy(kTinyTime);
  EXPECT_FALSE(host_resolver_.has_pending_requests());
  EXPECT_TRUE(test_delegate.has_result());
  EXPECT_THAT(test_delegate.WaitForResult(), test::IsError(ERR_TIMED_OUT));
}

// Check initial priority is passed to the HostResolver, and priority can be
// modified.
TEST_F(SOCKSConnectJobTest, Priority) {
  host_resolver_.set_ondemand_mode(true);
  for (int initial_priority = MINIMUM_PRIORITY;
       initial_priority <= MAXIMUM_PRIORITY; ++initial_priority) {
    for (int new_priority = MINIMUM_PRIORITY; new_priority <= MAXIMUM_PRIORITY;
         ++new_priority) {
      // Don't try changing priority to itself, as APIs may not allow that.
      if (new_priority == initial_priority) {
        continue;
      }
      TestConnectJobDelegate test_delegate;
      SOCKSConnectJob socks_connect_job(
          static_cast<RequestPriority>(initial_priority), SocketTag(),
          &common_connect_job_params_, CreateSOCKSParams(SOCKSVersion::V4),
          &test_delegate, nullptr /* net_log */);
      ASSERT_THAT(socks_connect_job.Connect(), test::IsError(ERR_IO_PENDING));
      ASSERT_TRUE(host_resolver_.has_pending_requests());
      int request_id = host_resolver_.num_resolve();
      EXPECT_EQ(initial_priority, host_resolver_.request_priority(request_id));

      // Change priority.
      socks_connect_job.ChangePriority(
          static_cast<RequestPriority>(new_priority));
      EXPECT_EQ(new_priority, host_resolver_.request_priority(request_id));

      // Restore initial priority.
      socks_connect_job.ChangePriority(
          static_cast<RequestPriority>(initial_priority));
      EXPECT_EQ(initial_priority, host_resolver_.request_priority(request_id));
    }
  }
}

TEST_F(SOCKSConnectJobTest, SecureDnsPolicy) {
  for (auto secure_dns_policy :
       {SecureDnsPolicy::kAllow, SecureDnsPolicy::kDisable}) {
    TestConnectJobDelegate test_delegate;
    SOCKSConnectJob socks_connect_job(
        DEFAULT_PRIORITY, SocketTag(), &common_connect_job_params_,
        CreateSOCKSParams(SOCKSVersion::V4, secure_dns_policy), &test_delegate,
        nullptr /* net_log */);
    ASSERT_THAT(socks_connect_job.Connect(), test::IsError(ERR_IO_PENDING));
    EXPECT_EQ(secure_dns_policy, host_resolver_.last_secure_dns_policy());
  }
}

TEST_F(SOCKSConnectJobTest, ConnectTiming) {
  host_resolver_.set_ondemand_mode(true);

  MockWrite writes[] = {
      MockWrite(ASYNC, ERR_IO_PENDING, 0),
      MockWrite(ASYNC, kSOCKS5GreetRequest, kSOCKS5GreetRequestLength, 1),
      MockWrite(SYNCHRONOUS, kSOCKS5OkRequest, kSOCKS5OkRequestLength, 3),
  };

  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kSOCKS5GreetResponse, kSOCKS5GreetResponseLength,
               2),
      MockRead(SYNCHRONOUS, kSOCKS5OkResponse, kSOCKS5OkResponseLength, 4),
  };

  SequencedSocketData sequenced_socket_data(reads, writes);
  // Host resolution is used to switch between sync and async connection
  // behavior. The SOCKS layer can't distinguish between sync and async host
  // resolution vs sync and async connection establishment, so just always
  // make connection establishment synchroonous.
  sequenced_socket_data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  client_socket_factory_.AddSocketDataProvider(&sequenced_socket_data);

  TestConnectJobDelegate test_delegate;
  SOCKSConnectJob socks_connect_job(DEFAULT_PRIORITY, SocketTag(),
                                    &common_connect_job_params_,
                                    CreateSOCKSParams(SOCKSVersion::V5),
                                    &test_delegate, nullptr /* net_log */);
  base::TimeTicks start = base::TimeTicks::Now();
  socks_connect_job.Connect();

  // DNS resolution completes after a short delay. The connection should be
  // immediately established as well. The first write to the socket stalls.
  FastForwardBy(kTinyTime);
  host_resolver_.ResolveAllPending();
  RunUntilIdle();

  // After another short delay, data is received from the server.
  FastForwardBy(kTinyTime);
  sequenced_socket_data.Resume();

  EXPECT_THAT(test_delegate.WaitForResult(), test::IsOk());
  // Proxy name resolution is not considered resolving the host name for
  // ConnectionInfo. For SOCKS4, where the host name is also looked up via DNS,
  // the resolution time is not currently reported.
  EXPECT_EQ(base::TimeTicks(),
            socks_connect_job.connect_timing().domain_lookup_start);
  EXPECT_EQ(base::TimeTicks(),
            socks_connect_job.connect_timing().domain_lookup_end);

  // The "connect" time for socks proxies includes DNS resolution time.
  EXPECT_EQ(start, socks_connect_job.connect_timing().connect_start);
  EXPECT_EQ(start + 2 * kTinyTime,
            socks_connect_job.connect_timing().connect_end);

  // Since SSL was not negotiated, SSL times are null.
  EXPECT_EQ(base::TimeTicks(), socks_connect_job.connect_timing().ssl_start);
  EXPECT_EQ(base::TimeTicks(), socks_connect_job.connect_timing().ssl_end);
}

TEST_F(SOCKSConnectJobTest, CancelDuringDnsResolution) {
  // Set HostResolver to hang.
  host_resolver_.set_ondemand_mode(true);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<SOCKSConnectJob> socks_connect_job =
      std::make_unique<SOCKSConnectJob>(DEFAULT_PRIORITY, SocketTag(),
                                        &common_connect_job_params_,
                                        CreateSOCKSParams(SOCKSVersion::V5),
                                        &test_delegate, nullptr /* net_log */);
  socks_connect_job->Connect();

  EXPECT_TRUE(host_resolver_.has_pending_requests());

  socks_connect_job.reset();
  RunUntilIdle();
  EXPECT_FALSE(host_resolver_.has_pending_requests());
  EXPECT_FALSE(test_delegate.has_result());
}

TEST_F(SOCKSConnectJobTest, CancelDuringConnect) {
  host_resolver_.set_synchronous_mode(true);

  SequencedSocketData sequenced_socket_data{base::span<MockRead>(),
                                            base::span<MockWrite>()};
  sequenced_socket_data.set_connect_data(MockConnect(ASYNC, OK));
  client_socket_factory_.AddSocketDataProvider(&sequenced_socket_data);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<SOCKSConnectJob> socks_connect_job =
      std::make_unique<SOCKSConnectJob>(DEFAULT_PRIORITY, SocketTag(),
                                        &common_connect_job_params_,
                                        CreateSOCKSParams(SOCKSVersion::V5),
                                        &test_delegate, nullptr /* net_log */);
  socks_connect_job->Connect();
  // Host resolution should resolve immediately. The ConnectJob should currently
  // be trying to connect.
  EXPECT_FALSE(host_resolver_.has_pending_requests());

  socks_connect_job.reset();
  RunUntilIdle();
  EXPECT_FALSE(test_delegate.has_result());
  // Socket should have been destroyed.
  EXPECT_FALSE(sequenced_socket_data.socket());
}

TEST_F(SOCKSConnectJobTest, CancelDuringHandshake) {
  host_resolver_.set_synchronous_mode(true);

  // Hang at start of handshake.
  MockWrite writes[] = {
      MockWrite(SYNCHRONOUS, ERR_IO_PENDING, 0),
  };
  SequencedSocketData sequenced_socket_data(base::span<MockRead>(), writes);
  sequenced_socket_data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  client_socket_factory_.AddSocketDataProvider(&sequenced_socket_data);

  TestConnectJobDelegate test_delegate;
  std::unique_ptr<SOCKSConnectJob> socks_connect_job =
      std::make_unique<SOCKSConnectJob>(DEFAULT_PRIORITY, SocketTag(),
                                        &common_connect_job_params_,
                                        CreateSOCKSParams(SOCKSVersion::V5),
                                        &test_delegate, nullptr /* net_log */);
  socks_connect_job->Connect();
  // Host resolution should resolve immediately. The socket connecting, and the
  // ConnectJob should currently be trying to send the SOCKS handshake.
  EXPECT_FALSE(host_resolver_.has_pending_requests());

  socks_connect_job.reset();
  RunUntilIdle();
  EXPECT_FALSE(test_delegate.has_result());
  // Socket should have been destroyed.
  EXPECT_FALSE(sequenced_socket_data.socket());
  EXPECT_TRUE(sequenced_socket_data.AllWriteDataConsumed());
}

}  // namespace
}  // namespace net
