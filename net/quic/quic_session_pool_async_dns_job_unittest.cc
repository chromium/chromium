// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/http/http_stream.h"
#include "net/http/http_stream_pool_test_util.h"
#include "net/log/test_net_log.h"
#include "net/quic/address_utils.h"
#include "net/quic/mock_crypto_client_stream.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_session_pool.h"
#include "net/quic/quic_session_pool_peer.h"
#include "net/quic/quic_session_pool_test_base.h"
#include "net/socket/socket_test_util.h"
#include "net/ssl/test_static_ech_mode_getter.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_error_codes.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace net::test {

namespace {

// Tests for QuicSessionPool::AsyncDnsJob. The job must be request-visibly
// equivalent to DirectJob, so the tests are parameterized over
// features::kAsyncQuicSession, which changes whether session creation is
// synchronous or asynchronous.
class QuicSessionPoolAsyncDnsJobTest : public QuicSessionPoolTestBase,
                                       public ::testing::TestWithParam<bool> {
 protected:
  static constexpr char kIpv6Addr1[] = "2001:db8::1";
  static constexpr char kIpv6Addr2[] = "2001:db8::2";
  static constexpr char kIpv4Addr1[] = "192.168.0.1";
  static constexpr char kIpv4Addr2[] = "192.168.0.2";

  // All features go through the base fixture, which settles them before any
  // test activity. A second ScopedFeatureList would swap the feature state
  // again while the task environment threads are already running.
  static std::vector<base::test::FeatureRef> EnabledFeatures(
      const std::vector<base::test::FeatureRef>& additional = {}) {
    std::vector<base::test::FeatureRef> enabled = additional;
    if (GetParam()) {
      enabled.push_back(features::kAsyncQuicSession);
    }
    return enabled;
  }

  static std::vector<base::test::FeatureRef> DisabledFeatures() {
    std::vector<base::test::FeatureRef> disabled;
    if (!GetParam()) {
      disabled.push_back(features::kAsyncQuicSession);
    }
    return disabled;
  }

  // The slow timer delay the job reads from the feature param.
  static base::TimeDelta SlowTimerDelay() {
    return features::kQuicSlowTimerDelay.Get();
  }

  QuicSessionPoolAsyncDnsJobTest()
      : QuicSessionPoolAsyncDnsJobTest(EnabledFeatures(),
                                       DisabledFeatures(),
                                       {{features::kAsyncDnsQuicJob, {}}}) {}

  QuicSessionPoolAsyncDnsJobTest(
      std::vector<base::test::FeatureRef> enabled_features,
      std::vector<base::test::FeatureRef> disabled_features,
      std::vector<base::test::FeatureRefAndParams> enabled_features_with_params)
      : QuicSessionPoolTestBase(
            DefaultSupportedQuicVersions().front(),
            std::move(enabled_features),
            std::move(disabled_features),
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            std::move(enabled_features_with_params)) {}

  bool async_quic_session() const { return GetParam(); }

  // Initializes the pool with `fake_resolver_` instead of `host_resolver_`.
  // Use for tests that drive partial resolver updates.
  void InitializeWithFakeResolver() {
    DCHECK(!pool_);
    pool_ = std::make_unique<QuicSessionPool>(
        net_log_.net_log(), &fake_resolver_, &ssl_config_service_,
        socket_factory_.get(), http_server_properties_.get(),
        cert_verifier_.get(), &transport_security_state_, proxy_delegate_.get(),
        /*sct_auditing_delegate=*/nullptr,
        /*SocketPerformanceWatcherFactory*/ nullptr,
        &crypto_client_stream_factory_, test_network_quality_estimator_.get(),
        &context_);
  }

  ServiceEndpoint MakeUsableEndpoint(std::string_view v4_addr) {
    return ServiceEndpointBuilder()
        .add_v4(v4_addr, kDefaultServerPort)
        .endpoint();
  }

  ServiceEndpoint MakeUsableEndpoint(std::string_view v4_addr1,
                                     std::string_view v4_addr2) {
    return ServiceEndpointBuilder()
        .add_v4(v4_addr1, kDefaultServerPort)
        .add_v4(v4_addr2, kDefaultServerPort)
        .endpoint();
  }

  ServiceEndpoint MakeUsableV6Endpoint(std::string_view v6_addr) {
    return ServiceEndpointBuilder()
        .add_v6(v6_addr, kDefaultServerPort)
        .endpoint();
  }

  IPEndPoint MakeIPEndPoint(std::string_view addr) {
    IPAddress ip_address;
    CHECK(ip_address.AssignFromIPLiteral(addr));
    return IPEndPoint(ip_address, kDefaultServerPort);
  }

  using SuccessSource = QuicSessionPoolPeer::AsyncDnsJob::SuccessSource;

  FakeServiceEndpointResolver fake_resolver_;
  // For NetLog events test coverage.
  RecordingNetLogObserver net_log_observer_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         QuicSessionPoolAsyncDnsJobTest,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "AsyncQuicSession"
                                             : "SyncQuicSession";
                         });

// DNS completes synchronously and the session is established without waiting
// for server responses (0-RTT).
TEST_P(QuicSessionPoolAsyncDnsJobTest, DnsSyncSessionEstablishmentSync) {
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(kDefaultServerHostName,
                                            "192.168.0.1", "");
  Initialize();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  int rv = builder.CallRequest();
  if (async_quic_session()) {
    // Session creation is asynchronous, so the request cannot complete
    // synchronously even though everything else is synchronous.
    EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
    EXPECT_THAT(callback_.WaitForResult(), IsOk());
  } else {
    EXPECT_THAT(rv, IsOk());
  }
  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();

  EXPECT_FALSE(
      net_log_observer_
          .GetEntriesWithType(
              NetLogEventType::QUIC_SESSION_POOL_ASYNC_DNS_JOB_ATTEMPT_STARTED)
          .empty());
  EXPECT_FALSE(
      net_log_observer_
          .GetEntriesWithType(
              NetLogEventType::QUIC_SESSION_POOL_ASYNC_DNS_JOB_COMPLETE)
          .empty());
}

// DNS completes synchronously; the crypto handshake completes asynchronously.
TEST_P(QuicSessionPoolAsyncDnsJobTest, DnsSyncSessionEstablishmentAsync) {
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(kDefaultServerHostName,
                                            "192.168.0.1", "");
  Initialize();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  // Host resolution has already finished, so the one-shot signal must not be
  // promised.
  TestCompletionCallback host_resolution_callback;
  EXPECT_FALSE(builder.request.WaitForHostResolution(
      host_resolution_callback.callback()));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
    // The session was created; its crypto handshake is still in flight.
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  } else {
    EXPECT_FALSE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  crypto_client_stream_factory_.last_stream()->NotifySessionZeroRttComplete();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// DNS completes asynchronously; everything after resolution completes without
// further asynchronous steps when session creation is synchronous.
TEST_P(QuicSessionPoolAsyncDnsJobTest, DnsAsyncSessionEstablishmentSync) {
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(builder.request.WaitForHostResolution(
      host_resolution_callback.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  // The one-shot host resolution signal carries the result of the progress
  // the job made after resolution.
  ASSERT_TRUE(host_resolution_callback.have_result());
  if (async_quic_session()) {
    EXPECT_THAT(host_resolution_callback.WaitForResult(),
                IsError(ERR_IO_PENDING));
  } else {
    EXPECT_THAT(host_resolution_callback.WaitForResult(), IsOk());
  }

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// DNS completes asynchronously and the crypto handshake completes
// asynchronously.
TEST_P(QuicSessionPoolAsyncDnsJobTest, DnsAsyncSessionEstablishmentAsync) {
  Initialize();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(builder.request.WaitForHostResolution(
      host_resolution_callback.callback()));
  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  } else {
    EXPECT_FALSE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  // An attempt is in flight when the host resolution signal fires.
  EXPECT_THAT(host_resolution_callback.WaitForResult(),
              IsError(ERR_IO_PENDING));
  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }

  crypto_client_stream_factory_.last_stream()->NotifySessionZeroRttComplete();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// DNS succeeds but provides zero endpoints. The job must fail with
// ERR_DNS_NO_MATCHING_SUPPORTED_ALPN, matching DirectJob.
TEST_P(QuicSessionPoolAsyncDnsJobTest, DnsSucceedsWithZeroEndpoints) {
  host_resolver_->rules()->AddRule(
      kDefaultServerHostName, MockHostResolverBase::RuleResolver::RuleResult(
                                  std::vector<HostResolverEndpointResult>()));
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback_.WaitForResult(),
              IsError(ERR_DNS_NO_MATCHING_SUPPORTED_ALPN));
  EXPECT_FALSE(HasActiveSession(kDefaultDestination));
}

// Same as the above, but DNS completes synchronously.
TEST_P(QuicSessionPoolAsyncDnsJobTest, DnsSucceedsWithZeroEndpointsSync) {
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddRule(
      kDefaultServerHostName, MockHostResolverBase::RuleResolver::RuleResult(
                                  std::vector<HostResolverEndpointResult>()));
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(),
              IsError(ERR_DNS_NO_MATCHING_SUPPORTED_ALPN));
  EXPECT_FALSE(HasActiveSession(kDefaultDestination));
}

// DNS provides endpoints, but none of them is usable for QUIC because no
// endpoint advertises a matching ALPN.
TEST_P(QuicSessionPoolAsyncDnsJobTest, NoEndpointUsableForQuic) {
  quic_params_->supported_versions = {version_};
  std::vector<HostResolverEndpointResult> endpoints(2);
  endpoints[0].ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  endpoints[0].metadata.supported_protocol_alpns = {"unknown"};
  // Add a final authority endpoint (no protocols specified) at the end.
  endpoints[1].ip_endpoints = {IPEndPoint(IPAddress::IPv4Localhost(), 0)};
  host_resolver_->rules()->AddRule(
      kDefaultServerHostName,
      MockHostResolverBase::RuleResolver::RuleResult(
          std::move(endpoints),
          /*aliases=*/std::set<std::string>{kDefaultServerHostName}));
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  RequestBuilder builder(this);
  builder.quic_version = quic::ParsedQuicVersion::Unsupported();
  builder.require_dns_https_alpn = true;
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback_.WaitForResult(),
              IsError(ERR_DNS_NO_MATCHING_SUPPORTED_ALPN));
  EXPECT_FALSE(HasActiveSession(kDefaultDestination));
}

// DNS provides multiple endpoints, but the first endpoint lacks QUIC ALPN
// support (e.g. "h2"). The job must skip the first endpoint and connect to the
// second usable endpoint.
TEST_P(QuicSessionPoolAsyncDnsJobTest, SkipsEndpointsWithoutAlpn) {
  quic_params_->supported_versions = {version_};
  std::vector<HostResolverEndpointResult> endpoints(2);
  endpoints[0].ip_endpoints = {IPEndPoint(IPAddress(192, 0, 2, 1), 443)};
  endpoints[0].metadata.supported_protocol_alpns = {"h2"};

  endpoints[1].ip_endpoints = {IPEndPoint(IPAddress(192, 0, 2, 2), 443)};
  endpoints[1].metadata.supported_protocol_alpns = {
      quic::AlpnForVersion(version_)};

  host_resolver_->rules()->AddRule(
      kDefaultServerHostName,
      MockHostResolverBase::RuleResolver::RuleResult(
          std::move(endpoints),
          /*aliases=*/std::set<std::string>{kDefaultServerHostName}));

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  builder.quic_version = quic::ParsedQuicVersion::Unsupported();
  builder.require_dns_https_alpn = true;

  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(
      kDefaultDestination, PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
      ProxyChain::Direct(), SessionUsage::kDestination,
      /*require_dns_https_alpn=*/true);
  ASSERT_TRUE(session);
  IPEndPoint peer_address;
  EXPECT_THAT(session->GetDefaultSocket()->GetPeerAddress(&peer_address),
              IsOk());
  EXPECT_EQ(peer_address, IPEndPoint(IPAddress(192, 0, 2, 2), 443));

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// DNS fails asynchronously. The job must fail with the resolver's error.
TEST_P(QuicSessionPoolAsyncDnsJobTest, DnsFailureAsync) {
  host_resolver_->rules()->AddSimulatedFailure(kDefaultServerHostName);
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(builder.request.WaitForHostResolution(
      host_resolution_callback.callback()));

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_NAME_NOT_RESOLVED));
  ASSERT_TRUE(host_resolution_callback.have_result());
  EXPECT_THAT(host_resolution_callback.WaitForResult(),
              IsError(ERR_NAME_NOT_RESOLVED));
}

// DNS fails synchronously. The request must fail without becoming pending.
TEST_P(QuicSessionPoolAsyncDnsJobTest, DnsFailureSync) {
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddSimulatedFailure(kDefaultServerHostName);
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_NAME_NOT_RESOLVED));

  TestCompletionCallback host_resolution_callback;
  EXPECT_FALSE(builder.request.WaitForHostResolution(
      host_resolution_callback.callback()));
}

// The request's initial priority must reach the resolver, and a priority
// change while resolution is in flight must be forwarded to it.
TEST_P(QuicSessionPoolAsyncDnsJobTest, PriorityReachesResolver) {
  host_resolver_->set_ondemand_mode(true);
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  builder.priority = MAXIMUM_PRIORITY;
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  EXPECT_EQ(MAXIMUM_PRIORITY, host_resolver_->last_request_priority());
  const size_t resolver_request_id = host_resolver_->last_id();
  EXPECT_EQ(MAXIMUM_PRIORITY,
            host_resolver_->request_priority(resolver_request_id));

  builder.request.SetPriority(LOWEST);
  EXPECT_EQ(LOWEST, host_resolver_->request_priority(resolver_request_id));

  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// Regression test for reentrant request addition: the host resolution
// notification for a failing job reenters the pool and adds a new request to
// the same job. The late-added request must not be promised the already-fired
// one-shot signals and must complete with the job's error.
TEST_P(QuicSessionPoolAsyncDnsJobTest, ReentrantAddRequestToFailedJob) {
  host_resolver_->rules()->AddSimulatedFailure(kDefaultServerHostName);
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  std::unique_ptr<RequestBuilder> builder2;
  TestCompletionCallback callback2;
  bool reentrant_callback_ran = false;
  EXPECT_TRUE(builder.request.WaitForHostResolution(
      base::BindLambdaForTesting([&](int rv) {
        EXPECT_THAT(rv, IsError(ERR_NAME_NOT_RESOLVED));
        // Reenter the pool and add a second request to the same job.
        builder2 = std::make_unique<RequestBuilder>(this);
        builder2->callback = callback2.callback();
        EXPECT_THAT(builder2->CallRequest(), IsError(ERR_IO_PENDING));
        // The one-shot signals have already fired or can no longer fire; the
        // late-added request must not wait on either.
        TestCompletionCallback stale_callback;
        EXPECT_FALSE(
            builder2->request.WaitForHostResolution(stale_callback.callback()));
        EXPECT_FALSE(builder2->request.WaitForQuicSessionCreation(
            stale_callback.callback()));
        reentrant_callback_ran = true;
      })));

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_TRUE(reentrant_callback_ran);
  EXPECT_THAT(callback2.WaitForResult(), IsError(ERR_NAME_NOT_RESOLVED));
}

// Regression test for reentrant request addition while the job is connecting:
// the host resolution notification reenters the pool and adds a new request
// to the same job. The late-added request must not be promised the host
// resolution signal, and its session creation expectation must be consistent
// with the job's state.
TEST_P(QuicSessionPoolAsyncDnsJobTest, ReentrantAddRequestToConnectingJob) {
  host_resolver_->set_ondemand_mode(true);
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  std::unique_ptr<RequestBuilder> builder2;
  TestCompletionCallback callback2;
  TestCompletionCallback creation_callback2;
  bool reentrant_callback_ran = false;
  EXPECT_TRUE(builder.request.WaitForHostResolution(
      base::BindLambdaForTesting([&](int rv) {
        builder2 = std::make_unique<RequestBuilder>(this);
        builder2->callback = callback2.callback();
        if (async_quic_session()) {
          // Session creation is still in flight; the new request joins the
          // job and is promised only the session creation signal.
          EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
          EXPECT_THAT(builder2->CallRequest(), IsError(ERR_IO_PENDING));
          TestCompletionCallback stale_callback;
          EXPECT_FALSE(builder2->request.WaitForHostResolution(
              stale_callback.callback()));
          EXPECT_TRUE(builder2->request.WaitForQuicSessionCreation(
              creation_callback2.callback()));
        } else {
          // The attempt completed synchronously after resolution, so the
          // session is already active and the new request uses it directly.
          EXPECT_THAT(rv, IsOk());
          EXPECT_THAT(builder2->CallRequest(), IsOk());
        }
        reentrant_callback_ran = true;
      })));

  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(reentrant_callback_ran);
  if (async_quic_session()) {
    EXPECT_THAT(creation_callback2.WaitForResult(), IsOk());
    EXPECT_THAT(callback2.WaitForResult(), IsOk());
  }

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&builder2->request);
  EXPECT_TRUE(stream2.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// Regression test for reentrant request cancellation/destruction during host
// resolution. When the callback to WaitForHostResolution destroys its own
// request (and it was the last request), the job is destroyed. The job must
// handle its own destruction gracefully without UAF or memory safety
// violations.
TEST_P(QuicSessionPoolAsyncDnsJobTest, ReentrantRemoveLastRequest) {
  host_resolver_->set_ondemand_mode(true);
  Initialize();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  auto builder = std::make_unique<RequestBuilder>(this);
  builder->callback = base::DoNothing();
  EXPECT_THAT(builder->CallRequest(), IsError(ERR_IO_PENDING));

  base::RunLoop run_loop;
  bool reentrant_callback_ran = false;
  EXPECT_TRUE(builder->request.WaitForHostResolution(
      base::BindLambdaForTesting([&](int rv) {
        reentrant_callback_ran = true;
        builder.reset();
        run_loop.Quit();
      })));

  host_resolver_->ResolveAllPending();
  run_loop.Run();
  EXPECT_TRUE(reentrant_callback_ran);
}

// Regression test for reentrant request cancellation/destruction during session
// creation notification.
TEST_P(QuicSessionPoolAsyncDnsJobTest,
       ReentrantRemoveRequestOnSessionCreation) {
  if (!async_quic_session()) {
    return;
  }
  host_resolver_->set_ondemand_mode(true);
  Initialize();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  auto builder = std::make_unique<RequestBuilder>(this);
  builder->callback = base::DoNothing();
  EXPECT_THAT(builder->CallRequest(), IsError(ERR_IO_PENDING));

  base::RunLoop run_loop;
  bool reentrant_callback_ran = false;
  EXPECT_TRUE(builder->request.WaitForQuicSessionCreation(
      base::BindLambdaForTesting([&](int rv) {
        reentrant_callback_ran = true;
        builder.reset();
        run_loop.Quit();
      })));

  host_resolver_->ResolveAllPending();
  run_loop.Run();
  EXPECT_TRUE(reentrant_callback_ran);
}

// A crypto-ready partial update with a usable endpoint starts connecting
// before the resolver finishes. Session establishment completes without
// further asynchronous steps when session creation is synchronous.
TEST_P(QuicSessionPoolAsyncDnsJobTest, PartialResultSessionEstablishmentSync) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  endpoint_request->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    // Session creation is asynchronous, so the update alone cannot complete
    // the request.
    EXPECT_FALSE(callback_.have_result());
  } else {
    // The attempt completed synchronously inside the update.
    EXPECT_TRUE(callback_.have_result());
  }
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  // The job completed without the resolver finishing, destroying the
  // resolver request.
  EXPECT_FALSE(endpoint_request);

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// A crypto-ready partial update starts an attempt whose crypto handshake
// completes asynchronously. The host resolution signal fires at attempt
// start with ERR_IO_PENDING, before the resolver finishes.
TEST_P(QuicSessionPoolAsyncDnsJobTest, PartialResultSessionEstablishmentAsync) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(builder.request.WaitForHostResolution(
      host_resolution_callback.callback()));
  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  } else {
    EXPECT_FALSE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  // An attempt started before the resolver finished.
  ASSERT_TRUE(host_resolution_callback.have_result());
  EXPECT_THAT(host_resolution_callback.WaitForResult(),
              IsError(ERR_IO_PENDING));
  EXPECT_TRUE(endpoint_request);
  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }

  crypto_client_stream_factory_.last_stream()->NotifySessionZeroRttComplete();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  // The job completed without the resolver finishing.
  EXPECT_FALSE(endpoint_request);

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// The host resolution signal fires once, at attempt start. The resolver
// finishing afterwards must not fire it again. A second fire would run an
// already-consumed one-shot callback and crash.
TEST_P(QuicSessionPoolAsyncDnsJobTest, HostResolutionSignalFiresOnce) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(builder.request.WaitForHostResolution(
      host_resolution_callback.callback()));
  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  ASSERT_TRUE(host_resolution_callback.have_result());
  EXPECT_THAT(host_resolution_callback.WaitForResult(),
              IsError(ERR_IO_PENDING));

  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  // The final result neither completed the job nor re-fired the signal.
  EXPECT_FALSE(callback_.have_result());

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }
  crypto_client_stream_factory_.last_stream()->NotifySessionZeroRttComplete();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();

  EXPECT_FALSE(
      net_log_observer_
          .GetEntriesWithType(
              NetLogEventType::
                  QUIC_SESSION_POOL_ASYNC_DNS_JOB_HOST_RESOLUTION_SIGNALED)
          .empty());
}

// A crypto-ready partial update with no endpoint usable for QUIC keeps the
// job waiting. A later update that adds a usable endpoint starts the
// attempt.
TEST_P(QuicSessionPoolAsyncDnsJobTest, PartialResultWithoutUsableEndpoint) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(builder.request.WaitForHostResolution(
      host_resolution_callback.callback()));
  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  // The endpoint only advertises an ALPN unusable for QUIC.
  endpoint_request->add_endpoint(ServiceEndpointBuilder()
                                     .add_v4("192.168.0.1", kDefaultServerPort)
                                     .set_alpns({"h2"})
                                     .endpoint());
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();
  EXPECT_FALSE(host_resolution_callback.have_result());
  EXPECT_FALSE(callback_.have_result());

  endpoint_request->add_endpoint(MakeUsableEndpoint("192.168.0.2"));
  endpoint_request->CallOnServiceEndpointsUpdated();
  ASSERT_TRUE(host_resolution_callback.have_result());
  EXPECT_THAT(host_resolution_callback.WaitForResult(),
              IsError(ERR_IO_PENDING));

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }
  crypto_client_stream_factory_.last_stream()->NotifySessionZeroRttComplete();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// A partial update that is not crypto ready does not start an attempt even
// when a usable endpoint is visible.
TEST_P(QuicSessionPoolAsyncDnsJobTest, PartialResultNotCryptoReady) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(builder.request.WaitForHostResolution(
      host_resolution_callback.callback()));
  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request->CallOnServiceEndpointsUpdated();
  EXPECT_FALSE(host_resolution_callback.have_result());
  EXPECT_FALSE(callback_.have_result());

  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();
  ASSERT_TRUE(host_resolution_callback.have_result());
  EXPECT_THAT(host_resolution_callback.WaitForResult(),
              IsError(ERR_IO_PENDING));

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }
  crypto_client_stream_factory_.last_stream()->NotifySessionZeroRttComplete();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// A resolver error arriving after an attempt started must not fail the job.
// The attempt succeeds and the request gets a session.
TEST_P(QuicSessionPoolAsyncDnsJobTest, LateResolverErrorIgnoredAttemptOk) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(builder.request.WaitForHostResolution(
      host_resolution_callback.callback()));
  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();
  EXPECT_THAT(host_resolution_callback.WaitForResult(),
              IsError(ERR_IO_PENDING));

  endpoint_request->CallOnServiceEndpointRequestFinished(ERR_NAME_NOT_RESOLVED);
  // The resolver error did not fail the job.
  EXPECT_FALSE(callback_.have_result());

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }
  crypto_client_stream_factory_.last_stream()->NotifySessionZeroRttComplete();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// A resolver error arriving after an attempt started must not fail the job.
// When the attempt then fails, the job fails with the attempt's error, not
// the resolver's.
TEST_P(QuicSessionPoolAsyncDnsJobTest, LateResolverErrorIgnoredAttemptFails) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // The session is closed before the handshake makes progress, so no write
  // is expected.
  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(builder.request.WaitForHostResolution(
      host_resolution_callback.callback()));
  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();
  EXPECT_THAT(host_resolution_callback.WaitForResult(),
              IsError(ERR_IO_PENDING));

  endpoint_request->CallOnServiceEndpointRequestFinished(ERR_NAME_NOT_RESOLVED);
  EXPECT_FALSE(callback_.have_result());

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }
  // Fail the attempt by closing the session mid handshake.
  GetPendingSession(kDefaultDestination)
      ->CloseSessionOnError(ERR_CONNECTION_REFUSED, quic::QUIC_INTERNAL_ERROR,
                            quic::ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_CONNECTION_REFUSED));
  EXPECT_FALSE(HasActiveSession(kDefaultDestination));

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// A crypto-ready partial update that matches an existing session by IP
// completes the job by pooling, without any attempt and before the resolver
// finishes.
TEST_P(QuicSessionPoolAsyncDnsJobTest, IpPoolingOnPartialResult) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request1 =
      fake_resolver_.AddFakeRequest();
  endpoint_request1->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request1->CompleteStartAsynchronously(OK);
  InitializeWithFakeResolver();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Establish a session to pool against.
  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request2 =
      fake_resolver_.AddFakeRequest();
  const url::SchemeHostPort server2(url::kHttpsScheme, kServer2HostName,
                                    kDefaultServerPort);
  // Scoped to the second job. The first job recorded its own entries.
  base::HistogramTester histograms;
  RequestBuilder builder2(this);
  builder2.destination = server2;
  builder2.url = GURL(kServer2Url);
  TestCompletionCallback callback2;
  builder2.callback = callback2.callback();
  EXPECT_THAT(builder2.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(builder2.request.WaitForHostResolution(
      host_resolution_callback.callback()));

  endpoint_request2->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request2->set_crypto_ready(true);
  endpoint_request2->CallOnServiceEndpointsUpdated();

  // The job pooled to the existing session inside the update, without an
  // attempt and before the resolver finished.
  EXPECT_TRUE(callback2.have_result());
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_THAT(host_resolution_callback.WaitForResult(), IsOk());
  EXPECT_FALSE(endpoint_request2);
  EXPECT_TRUE(HasActiveSession(server2));

  std::unique_ptr<HttpStream> stream2 = CreateStream(&builder2.request);
  EXPECT_TRUE(stream2.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();

  histograms.ExpectUniqueSample("Net.QuicSession.AsyncDnsJob.SuccessSource",
                                static_cast<int>(SuccessSource::kIpPooling), 1);
  histograms.ExpectUniqueSample(
      "Net.QuicSession.AsyncDnsJob.AttemptsPerJob.JobSucceeded", 0, 1);
  histograms.ExpectTotalCount(
      "Net.QuicSession.AsyncDnsJob.SuccessfulAttemptElapsedTime."
      "FinalDnsResult",
      0);
  histograms.ExpectTotalCount(
      "Net.QuicSession.AsyncDnsJob.SuccessfulAttemptElapsedTime."
      "JobSuccessWithDnsInFlight",
      0);
}

// A priority change made after an attempt started but while resolution is
// still in flight reaches the resolver. Changes after the resolver finished
// do not.
TEST_P(QuicSessionPoolAsyncDnsJobTest, PriorityChangeWhileAttemptInFlight) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  builder.priority = MAXIMUM_PRIORITY;
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));
  EXPECT_EQ(MAXIMUM_PRIORITY, endpoint_request->priority());

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  // The attempt started but resolution is still in flight.
  builder.request.SetPriority(LOWEST);
  EXPECT_EQ(LOWEST, endpoint_request->priority());

  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  builder.request.SetPriority(HIGHEST);
  EXPECT_EQ(LOWEST, endpoint_request->priority());

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }
  crypto_client_stream_factory_.last_stream()->NotifySessionZeroRttComplete();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// An IP pooling miss is recorded for the first checked endpoints only.
// Later updates do not add miss entries, and a pooling hit is still
// recorded.
TEST_P(QuicSessionPoolAsyncDnsJobTest, IpPoolingMissRecordedOnce) {
  constexpr std::string_view kHistogram =
      "Net.QuicSession.FindMatchingIpSessionResult";

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request1 =
      fake_resolver_.AddFakeRequest();
  endpoint_request1->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request1->CompleteStartAsynchronously(OK);
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Establish a session to pool against.
  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  client_maker_.Reset();
  MockQuicData socket_data2(version_);
  socket_data2.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data2.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  base::HistogramTester histograms;

  // A second job whose endpoint does not match the existing session by IP.
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request2 =
      fake_resolver_.AddFakeRequest();
  const url::SchemeHostPort server2(url::kHttpsScheme, kServer2HostName,
                                    kDefaultServerPort);
  RequestBuilder builder2(this);
  builder2.destination = server2;
  builder2.url = GURL(kServer2Url);
  TestCompletionCallback callback2;
  builder2.callback = callback2.callback();
  EXPECT_THAT(builder2.CallRequest(), IsError(ERR_IO_PENDING));
  TestCompletionCallback creation_callback2;
  if (async_quic_session()) {
    EXPECT_TRUE(builder2.request.WaitForQuicSessionCreation(
        creation_callback2.callback()));
  }

  endpoint_request2->add_endpoint(MakeUsableEndpoint("10.0.0.1"));
  endpoint_request2->set_crypto_ready(true);
  endpoint_request2->CallOnServiceEndpointsUpdated();

  // The miss was recorded once per address family list of the endpoint.
  // The recorded sample is CAN_POOL_BUT_DIFFERENT_IP.
  histograms.ExpectTotalCount(kHistogram, 2);
  histograms.ExpectBucketCount(kHistogram, /*sample=*/1, 2);

  // Further updates and the final result do not add miss entries.
  endpoint_request2->add_endpoint(MakeUsableEndpoint("10.0.0.2"));
  endpoint_request2->CallOnServiceEndpointsUpdated();
  histograms.ExpectTotalCount(kHistogram, 2);
  endpoint_request2->CallOnServiceEndpointRequestFinished(OK);
  histograms.ExpectTotalCount(kHistogram, 2);

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback2.WaitForResult(), IsError(ERR_IO_PENDING));
  }
  crypto_client_stream_factory_.last_stream()->NotifySessionZeroRttComplete();
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&builder2.request);
  EXPECT_TRUE(stream2.get());

  // A third job whose endpoint matches the first session by IP records a
  // pooling hit.
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request3 =
      fake_resolver_.AddFakeRequest();
  const url::SchemeHostPort server3(url::kHttpsScheme, kServer3HostName,
                                    kDefaultServerPort);
  RequestBuilder builder3(this);
  builder3.destination = server3;
  builder3.url = GURL(kServer3Url);
  TestCompletionCallback callback3;
  builder3.callback = callback3.callback();
  EXPECT_THAT(builder3.CallRequest(), IsError(ERR_IO_PENDING));

  endpoint_request3->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request3->set_crypto_ready(true);
  endpoint_request3->CallOnServiceEndpointsUpdated();

  // The recorded sample is MATCHING_IP_SESSION_FOUND.
  histograms.ExpectBucketCount(kHistogram, /*sample=*/0, 1);
  EXPECT_THAT(callback3.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(server3));

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
  socket_data2.ExpectAllReadDataConsumed();
  socket_data2.ExpectAllWriteDataConsumed();
}

// Regression test for reentrant request addition when the host resolution
// notification fires at attempt start on a partial result. The late-added
// request must not be promised the host resolution signal, and its session
// creation expectation must be consistent with the job's state.
TEST_P(QuicSessionPoolAsyncDnsJobTest, ReentrantAddRequestOnPartialResult) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  std::unique_ptr<RequestBuilder> builder2;
  TestCompletionCallback callback2;
  TestCompletionCallback creation_callback2;
  bool reentrant_callback_ran = false;
  EXPECT_TRUE(builder.request.WaitForHostResolution(
      base::BindLambdaForTesting([&](int rv) {
        builder2 = std::make_unique<RequestBuilder>(this);
        builder2->callback = callback2.callback();
        if (async_quic_session()) {
          // Session creation is still in flight; the new request joins the
          // job and is promised only the session creation signal.
          EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
          EXPECT_THAT(builder2->CallRequest(), IsError(ERR_IO_PENDING));
          TestCompletionCallback stale_callback;
          EXPECT_FALSE(builder2->request.WaitForHostResolution(
              stale_callback.callback()));
          EXPECT_TRUE(builder2->request.WaitForQuicSessionCreation(
              creation_callback2.callback()));
        } else {
          // The attempt completed synchronously inside the update, so the
          // session is already active and the new request uses it directly.
          EXPECT_THAT(rv, IsOk());
          EXPECT_THAT(builder2->CallRequest(), IsOk());
        }
        reentrant_callback_ran = true;
      })));

  endpoint_request->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(reentrant_callback_ran);
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  if (async_quic_session()) {
    EXPECT_THAT(creation_callback2.WaitForResult(), IsOk());
    EXPECT_THAT(callback2.WaitForResult(), IsOk());
  }

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&builder2->request);
  EXPECT_TRUE(stream2.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// The first candidate fails while its attempt is starting. The connector
// advances to the second candidate, which succeeds.
TEST_P(QuicSessionPoolAsyncDnsJobTest,
       SecondCandidateSucceedsAfterAttemptStartFailure) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);

  MockQuicData failing_socket_data(version_);
  failing_socket_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  failing_socket_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(
      MakeUsableEndpoint("192.168.0.1", "192.168.0.2"));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  if (async_quic_session()) {
    // The first attempt's failed session creation was held, so the requests
    // see the second attempt's result.
    EXPECT_THAT(creation_callback.WaitForResult(), IsOk());
  }

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  failing_socket_data.ExpectAllReadDataConsumed();
  failing_socket_data.ExpectAllWriteDataConsumed();
  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// The first candidate creates a session whose handshake then fails. The
// connector advances to the second candidate, which succeeds.
TEST_P(QuicSessionPoolAsyncDnsJobTest,
       SecondCandidateSucceedsAfterHandshakeFailure) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // The first session is closed before its handshake makes progress, so no
  // write is expected on it.
  MockQuicData failing_socket_data(version_);
  failing_socket_data.AddReadPauseForever();
  failing_socket_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(
      MakeUsableEndpoint("192.168.0.1", "192.168.0.2"));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }

  // The second attempt completes without waiting for its handshake.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  GetPendingSession(kDefaultDestination)
      ->CloseSessionOnError(ERR_CONNECTION_REFUSED, quic::QUIC_INTERNAL_ERROR,
                            quic::ConnectionCloseBehavior::SILENT_CLOSE);

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(kDefaultDestination));

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  failing_socket_data.ExpectAllReadDataConsumed();
  failing_socket_data.ExpectAllWriteDataConsumed();
  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// Every candidate failed and resolution finished. The job reports the result
// and the error details of the most recently failed attempt.
TEST_P(QuicSessionPoolAsyncDnsJobTest,
       AllCandidatesFailAfterResolutionFinished) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData failing_socket_data(version_);
  failing_socket_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  failing_socket_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(
      MakeUsableEndpoint("192.168.0.1", "192.168.0.2"));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    // The first attempt's failed session creation was held. The second
    // attempt created its session.
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }

  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  // The second attempt is still in flight, so the job did not settle.
  EXPECT_FALSE(callback_.have_result());

  GetPendingSession(kDefaultDestination)
      ->CloseSessionOnError(ERR_CONNECTION_REFUSED,
                            quic::QUIC_PACKET_WRITE_ERROR,
                            quic::ConnectionCloseBehavior::SILENT_CLOSE);

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_CONNECTION_REFUSED));
  EXPECT_FALSE(HasActiveSession(kDefaultDestination));
  // The details come from the second attempt, which had a session. The first
  // attempt failed before creating one, so it has no connection info.
  EXPECT_EQ(builder.net_error_details.connection_info,
            QuicHttpStream::ConnectionInfoFromQuicVersion(version_));

  failing_socket_data.ExpectAllReadDataConsumed();
  failing_socket_data.ExpectAllWriteDataConsumed();
  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// Every candidate fails while creating its session. The held creation result
// reaches the requests once the job's failure is decisive, and it is the one
// of the last attempt.
TEST_P(QuicSessionPoolAsyncDnsJobTest,
       HeldSessionCreationFailureDeliveredOnJobFailure) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData first_socket_data(version_);
  first_socket_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  first_socket_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData second_socket_data(version_);
  second_socket_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_INVALID);
  second_socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(
      MakeUsableEndpoint("192.168.0.1", "192.168.0.2"));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_ADDRESS_INVALID));
  EXPECT_FALSE(HasActiveSession(kDefaultDestination));
  if (async_quic_session()) {
    // The first attempt's failure was held and superseded by the second one.
    EXPECT_THAT(creation_callback.WaitForResult(),
                IsError(ERR_ADDRESS_INVALID));
  }

  first_socket_data.ExpectAllReadDataConsumed();
  first_socket_data.ExpectAllWriteDataConsumed();
  second_socket_data.ExpectAllReadDataConsumed();
  second_socket_data.ExpectAllWriteDataConsumed();
}

// The connector runs out of untried candidates while resolution is still in
// flight. The job waits, and a later update that supplies a candidate lets it
// succeed.
TEST_P(QuicSessionPoolAsyncDnsJobTest,
       LaterUpdateSuppliesCandidateAfterExhaustion) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData failing_socket_data(version_);
  failing_socket_data.AddReadPauseForever();
  failing_socket_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback host_resolution_callback;
  EXPECT_TRUE(builder.request.WaitForHostResolution(
      host_resolution_callback.callback()));
  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  EXPECT_THAT(host_resolution_callback.WaitForResult(),
              IsError(ERR_IO_PENDING));
  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }

  // The only candidate fails. Resolution is still in flight, so running out
  // of candidates is not terminal.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  GetPendingSession(kDefaultDestination)
      ->CloseSessionOnError(ERR_CONNECTION_REFUSED, quic::QUIC_INTERNAL_ERROR,
                            quic::ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_FALSE(callback_.have_result());
  ASSERT_TRUE(endpoint_request);

  endpoint_request->add_endpoint(MakeUsableEndpoint("192.168.0.2"));
  endpoint_request->CallOnServiceEndpointsUpdated();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(kDefaultDestination));

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  failing_socket_data.ExpectAllReadDataConsumed();
  failing_socket_data.ExpectAllWriteDataConsumed();
  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// The connector ran out of candidates and the resolver then finished with an
// error and no results. The job reports the failed attempt's result, not the
// resolver's error and not the empty results error.
TEST_P(QuicSessionPoolAsyncDnsJobTest, ResolverErrorAfterExhaustion) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // The session is closed before its handshake makes progress, so no write is
  // expected.
  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }

  // The only candidate fails, so the connector waits for more results.
  GetPendingSession(kDefaultDestination)
      ->CloseSessionOnError(ERR_CONNECTION_REFUSED, quic::QUIC_INTERNAL_ERROR,
                            quic::ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_FALSE(callback_.have_result());
  ASSERT_TRUE(endpoint_request);

  // The resolver drops its results when it fails.
  endpoint_request->set_endpoints({});
  endpoint_request->CallOnServiceEndpointRequestFinished(ERR_NAME_NOT_RESOLVED);

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_CONNECTION_REFUSED));
  EXPECT_FALSE(HasActiveSession(kDefaultDestination));
  EXPECT_EQ(builder.net_error_details.connection_info,
            QuicHttpStream::ConnectionInfoFromQuicVersion(version_));

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// An untried IPv6 candidate is attempted before an IPv4 one, even when the
// IPv4 endpoint comes first. IPv4 is attempted once the IPv6 candidate
// failed.
TEST_P(QuicSessionPoolAsyncDnsJobTest, PrefersIpv6BeforeIpv4) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData failing_socket_data(version_);
  failing_socket_data.AddReadPauseForever();
  failing_socket_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request->add_endpoint(ServiceEndpointBuilder()
                                     .add_v6("2001:db8::1", kDefaultServerPort)
                                     .endpoint());
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }

  QuicChromiumClientSession* ipv6_session =
      GetPendingSession(kDefaultDestination);
  EXPECT_EQ(ToIPEndPoint(ipv6_session->peer_address()),
            MakeIPEndPoint("2001:db8::1"));

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  ipv6_session->CloseSessionOnError(
      ERR_CONNECTION_REFUSED, quic::QUIC_INTERNAL_ERROR,
      quic::ConnectionCloseBehavior::SILENT_CLOSE);

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint("192.168.0.1"));

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  failing_socket_data.ExpectAllReadDataConsumed();
  failing_socket_data.ExpectAllWriteDataConsumed();
  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// An IP that a later update delivers again is not attempted a second time.
// Only one socket is provided, so a second attempt would fail the test.
TEST_P(QuicSessionPoolAsyncDnsJobTest, ClaimedIpEndPointNotRetried) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }

  GetPendingSession(kDefaultDestination)
      ->CloseSessionOnError(ERR_CONNECTION_REFUSED, quic::QUIC_INTERNAL_ERROR,
                            quic::ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_FALSE(callback_.have_result());
  ASSERT_TRUE(endpoint_request);

  // The same IP arrives again under another endpoint.
  endpoint_request->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request->CallOnServiceEndpointsUpdated();
  EXPECT_FALSE(callback_.have_result());

  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_CONNECTION_REFUSED));
  EXPECT_FALSE(HasActiveSession(kDefaultDestination));

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// Two endpoints carry the same IP with different metadata. The attempt to the
// first of them fails, and the connector attempts the second one, because the
// metadata decides how the handshake runs.
TEST_P(QuicSessionPoolAsyncDnsJobTest, MetadataVariantOfClaimedIpIsAttempted) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  base::HistogramTester histograms;
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);

  // The attempt to the first variant fails while it starts.
  MockQuicData failing_socket_data(version_);
  failing_socket_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  failing_socket_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  // Both endpoints hold the same IP and differ in their ECH config.
  endpoint_request->add_endpoint(ServiceEndpointBuilder()
                                     .add_v4("192.168.0.1", kDefaultServerPort)
                                     .set_alpn(version_)
                                     .set_ech_config_list({1, 2, 3})
                                     .endpoint());
  endpoint_request->add_endpoint(ServiceEndpointBuilder()
                                     .add_v4("192.168.0.1", kDefaultServerPort)
                                     .set_alpn(version_)
                                     .set_ech_config_list({4, 5, 6})
                                     .endpoint());
  endpoint_request->set_crypto_ready(true);
  // Resolution finishes here, so running out of candidates would fail the
  // job instead of leaving it waiting.
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsOk());
  }
  EXPECT_TRUE(HasActiveSession(kDefaultDestination));

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  // Both sockets were used, so the second variant was not skipped.
  failing_socket_data.ExpectAllReadDataConsumed();
  failing_socket_data.ExpectAllWriteDataConsumed();
  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();

  histograms.ExpectUniqueSample(
      "Net.QuicSession.AsyncDnsJob.SuccessSource",
      static_cast<int>(SuccessSource::kInitialConnectorLaterAttempt), 1);
  histograms.ExpectUniqueSample(
      "Net.QuicSession.AsyncDnsJob.AttemptsPerJob.JobSucceeded", 2, 1);
}

// An endpoint that matches an existing session by IP arrives after an attempt
// started. The job pools to that session instead of attempting the endpoint,
// and the re-check adds no miss entry to the metric.
TEST_P(QuicSessionPoolAsyncDnsJobTest, IpPoolingCheckedBeforeNextAttempt) {
  constexpr std::string_view kHistogram =
      "Net.QuicSession.FindMatchingIpSessionResult";

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request1 =
      fake_resolver_.AddFakeRequest();
  endpoint_request1->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request1->CompleteStartAsynchronously(OK);
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Establish a session to pool against.
  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  MockQuicData failing_socket_data(version_);
  failing_socket_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  failing_socket_data.AddSocketDataToFactory(socket_factory_.get());

  base::HistogramTester histograms;

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request2 =
      fake_resolver_.AddFakeRequest();
  const url::SchemeHostPort server2(url::kHttpsScheme, kServer2HostName,
                                    kDefaultServerPort);
  RequestBuilder builder2(this);
  builder2.destination = server2;
  builder2.url = GURL(kServer2Url);
  TestCompletionCallback callback2;
  builder2.callback = callback2.callback();
  EXPECT_THAT(builder2.CallRequest(), IsError(ERR_IO_PENDING));

  endpoint_request2->add_endpoint(MakeUsableEndpoint("10.0.0.1"));
  endpoint_request2->set_crypto_ready(true);
  endpoint_request2->CallOnServiceEndpointsUpdated();

  // The miss was recorded once per address family list of the endpoint.
  // The recorded sample is CAN_POOL_BUT_DIFFERENT_IP.
  histograms.ExpectTotalCount(kHistogram, 2);
  histograms.ExpectBucketCount(kHistogram, /*sample=*/1, 2);

  // The matching endpoint arrives after the attempt to 10.0.0.1 started.
  endpoint_request2->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request2->CallOnServiceEndpointsUpdated();

  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_TRUE(HasActiveSession(server2));

  // Only the pooling hit was added. The recorded sample is
  // MATCHING_IP_SESSION_FOUND.
  histograms.ExpectTotalCount(kHistogram, 3);
  histograms.ExpectBucketCount(kHistogram, /*sample=*/0, 1);
  histograms.ExpectBucketCount(kHistogram, /*sample=*/1, 2);

  std::unique_ptr<HttpStream> stream2 = CreateStream(&builder2.request);
  EXPECT_TRUE(stream2.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
  failing_socket_data.ExpectAllReadDataConsumed();
  failing_socket_data.ExpectAllWriteDataConsumed();
}

// Regression test for reentrant request addition while the connector walks
// its candidates. The late-added request must be promised only the signals
// that can still fire, and must complete with the job's error.
TEST_P(QuicSessionPoolAsyncDnsJobTest, ReentrantAddRequestWhileAdvancing) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData first_socket_data(version_);
  first_socket_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  first_socket_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData second_socket_data(version_);
  second_socket_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_INVALID);
  second_socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  std::unique_ptr<RequestBuilder> builder2;
  TestCompletionCallback callback2;
  TestCompletionCallback creation_callback2;
  bool reentrant_callback_ran = false;
  EXPECT_TRUE(builder.request.WaitForHostResolution(
      base::BindLambdaForTesting([&](int rv) {
        builder2 = std::make_unique<RequestBuilder>(this);
        builder2->callback = callback2.callback();
        TestCompletionCallback stale_callback;
        if (async_quic_session()) {
          // The first attempt is creating its session, so the new request
          // joins the job and is promised only the session creation signal.
          EXPECT_THAT(rv, IsError(ERR_IO_PENDING));
          EXPECT_THAT(builder2->CallRequest(), IsError(ERR_IO_PENDING));
          EXPECT_FALSE(builder2->request.WaitForHostResolution(
              stale_callback.callback()));
          EXPECT_TRUE(builder2->request.WaitForQuicSessionCreation(
              creation_callback2.callback()));
        } else {
          // Every candidate already failed, so neither signal can fire for
          // the new request.
          EXPECT_THAT(rv, IsError(ERR_ADDRESS_INVALID));
          EXPECT_THAT(builder2->CallRequest(), IsError(ERR_IO_PENDING));
          EXPECT_FALSE(builder2->request.WaitForHostResolution(
              stale_callback.callback()));
          EXPECT_FALSE(builder2->request.WaitForQuicSessionCreation(
              stale_callback.callback()));
        }
        reentrant_callback_ran = true;
      })));

  endpoint_request->add_endpoint(
      MakeUsableEndpoint("192.168.0.1", "192.168.0.2"));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_ADDRESS_INVALID));
  EXPECT_TRUE(reentrant_callback_ran);
  EXPECT_THAT(callback2.WaitForResult(), IsError(ERR_ADDRESS_INVALID));
  if (async_quic_session()) {
    EXPECT_THAT(creation_callback2.WaitForResult(),
                IsError(ERR_ADDRESS_INVALID));
  }

  first_socket_data.ExpectAllReadDataConsumed();
  first_socket_data.ExpectAllWriteDataConsumed();
  second_socket_data.ExpectAllReadDataConsumed();
  second_socket_data.ExpectAllWriteDataConsumed();
}

// The slow timer fires while the primary connector's IPv6 attempt is in
// flight. The secondary connector attempts IPv4 and succeeds, so the requests
// get its session.
TEST_P(QuicSessionPoolAsyncDnsJobTest, SlowTimerStartsSecondaryThatSucceeds) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  base::HistogramTester histograms;
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // The IPv6 attempt never finishes its handshake.
  MockQuicData ipv6_data(version_);
  ipv6_data.AddReadPauseForever();
  ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData ipv4_data(version_);
  ipv4_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  ipv4_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }
  // The primary connector prefers the IPv6 candidate.
  QuicChromiumClientSession* ipv6_session =
      GetPendingSession(kDefaultDestination);
  EXPECT_EQ(ToIPEndPoint(ipv6_session->peer_address()),
            MakeIPEndPoint(kIpv6Addr1));

  // The secondary connector starts an IPv4 attempt when the timer fires.
  FastForwardBy(SlowTimerDelay());
  EXPECT_FALSE(callback_.have_result());
  ASSERT_EQ(crypto_client_stream_factory_.streams().size(), 2u);

  constexpr base::TimeDelta kTimeUntilSuccess = base::Milliseconds(20);
  FastForwardBy(kTimeUntilSuccess);
  crypto_client_stream_factory_.streams()[1]->NotifySessionZeroRttComplete();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv4Addr1));
  // The job is gone, and the primary connector cancelled the attempt it had
  // in flight on the way out. The session that attempt created is closed,
  // not left behind.
  EXPECT_FALSE(HasActiveJob(kDefaultDestination, PRIVACY_MODE_DISABLED));
  EXPECT_FALSE(QuicSessionPoolPeer::IsLiveSession(pool_.get(), ipv6_session));

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  ipv4_data.ExpectAllReadDataConsumed();
  ipv4_data.ExpectAllWriteDataConsumed();

  EXPECT_FALSE(
      net_log_observer_
          .GetEntriesWithType(
              NetLogEventType::QUIC_SESSION_POOL_ASYNC_DNS_JOB_SLOW_TIMER_ARMED)
          .empty());
  EXPECT_FALSE(
      net_log_observer_
          .GetEntriesWithType(
              NetLogEventType::QUIC_SESSION_POOL_ASYNC_DNS_JOB_SLOW_TIMER_FIRED)
          .empty());

  histograms.ExpectUniqueSample(
      "Net.QuicSession.AsyncDnsJob.SuccessSource",
      static_cast<int>(SuccessSource::kSlowTimerConnector), 1);
  histograms.ExpectUniqueSample(
      "Net.QuicSession.AsyncDnsJob.AttemptsPerJob.JobSucceeded", 2, 1);
  histograms.ExpectTimeBucketCount(
      "Net.QuicSession.AsyncDnsJob.SuccessfulAttemptElapsedTime."
      "JobSuccessWithDnsInFlight",
      kTimeUntilSuccess, 1);
  histograms.ExpectTotalCount(
      "Net.QuicSession.AsyncDnsJob.SuccessfulAttemptElapsedTime."
      "FinalDnsResult",
      0);
}

// The primary connector succeeds after the secondary connector started its
// own attempt. The in-flight attempt of the secondary connector is destroyed.
TEST_P(QuicSessionPoolAsyncDnsJobTest, PrimarySucceedsAfterSecondaryStarted) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  base::HistogramTester histograms;
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData ipv6_data(version_);
  ipv6_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  ipv6_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  // The IPv4 attempt never finishes its handshake.
  MockQuicData ipv4_data(version_);
  ipv4_data.AddReadPauseForever();
  ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }

  // Both connectors now have an attempt in flight.
  FastForwardBy(SlowTimerDelay());
  EXPECT_FALSE(callback_.have_result());
  ASSERT_EQ(crypto_client_stream_factory_.streams().size(), 2u);

  // Finish DNS while both attempts are in flight.
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  // Finish the handshake of the attempt the primary connector started.
  crypto_client_stream_factory_.streams()[0]->NotifySessionZeroRttComplete();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv6Addr1));
  // The job is gone, so the secondary connector and the attempt it had in
  // flight are gone with it.
  EXPECT_FALSE(HasActiveJob(kDefaultDestination, PRIVACY_MODE_DISABLED));

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  ipv6_data.ExpectAllReadDataConsumed();
  ipv6_data.ExpectAllWriteDataConsumed();

  // The initial connector's first attempt succeeds. The secondary attempt is
  // included in the attempt count, but does not change the source.
  histograms.ExpectUniqueSample(
      "Net.QuicSession.AsyncDnsJob.SuccessSource",
      static_cast<int>(SuccessSource::kInitialConnectorFirstAttempt), 1);
  histograms.ExpectUniqueSample(
      "Net.QuicSession.AsyncDnsJob.AttemptsPerJob.JobSucceeded", 2, 1);
  histograms.ExpectTimeBucketCount(
      "Net.QuicSession.AsyncDnsJob.SuccessfulAttemptElapsedTime."
      "FinalDnsResult",
      SlowTimerDelay(), 1);
  histograms.ExpectTotalCount(
      "Net.QuicSession.AsyncDnsJob.SuccessfulAttemptElapsedTime."
      "JobSuccessWithDnsInFlight",
      0);
}

// The slow timer fires while the primary connector waits for a candidate.
// The secondary slot is filled anyway, so the two candidates a later update
// supplies are attempted at once and the IPv4 one succeeds without waiting
// for the IPv6 one.
TEST_P(QuicSessionPoolAsyncDnsJobTest,
       SlowTimerFillsSecondarySlotWhileWaiting) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // The only candidate of the first update fails before the timer fires.
  MockQuicData first_ipv6_data(version_);
  first_ipv6_data.AddReadPause();
  first_ipv6_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);
  first_ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  // The late IPv6 attempt never finishes its handshake.
  MockQuicData second_ipv6_data(version_);
  second_ipv6_data.AddReadPauseForever();
  second_ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData ipv4_data(version_);
  ipv4_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  ipv4_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }

  // The primary connector runs out of candidates while resolution is still in
  // flight, so it waits with nothing in flight.
  first_ipv6_data.Resume();
  EXPECT_FALSE(callback_.have_result());

  // The timer fills the secondary slot even though no attempt is running.
  FastForwardBy(SlowTimerDelay());
  EXPECT_EQ(crypto_client_stream_factory_.streams().size(), 1u);

  // One connector takes the IPv6 candidate and the other takes the IPv4 one,
  // so both attempts run at once.
  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr2));
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->CallOnServiceEndpointsUpdated();
  crypto_client_stream_factory_.WaitForStreams(3);
  EXPECT_FALSE(callback_.have_result());

  // The IPv4 attempt finishes its handshake while the IPv6 attempt still
  // hangs.
  crypto_client_stream_factory_.streams()[2]->NotifySessionZeroRttComplete();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv4Addr1));

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  ipv4_data.ExpectAllReadDataConsumed();
  ipv4_data.ExpectAllWriteDataConsumed();
}

// The slow timer fires while no IPv4 candidate is visible. The secondary
// connector waits, and starts its attempt when a later update supplies one.
TEST_P(QuicSessionPoolAsyncDnsJobTest, SecondaryWaitsForLaterIpv4Candidate) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData ipv6_data(version_);
  ipv6_data.AddReadPauseForever();
  ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData ipv4_data(version_);
  ipv4_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  ipv4_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }

  // The secondary connector has nothing to attempt yet.
  FastForwardBy(SlowTimerDelay());
  EXPECT_FALSE(callback_.have_result());
  EXPECT_EQ(crypto_client_stream_factory_.streams().size(), 1u);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->CallOnServiceEndpointsUpdated();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv4Addr1));

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  ipv4_data.ExpectAllReadDataConsumed();
  ipv4_data.ExpectAllWriteDataConsumed();
}

// Only IPv4 is visible when the primary connector starts, so it attempts
// IPv4. When the slow timer fires the connectors change slots, and the
// connector that moved into the primary slot attempts a late IPv6 candidate.
TEST_P(QuicSessionPoolAsyncDnsJobTest, SlotSwapWhenPrimaryAttemptsIpv4) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  base::HistogramTester histograms;
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // The IPv4 attempt never finishes its handshake.
  MockQuicData ipv4_data(version_);
  ipv4_data.AddReadPauseForever();
  ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData ipv6_data(version_);
  ipv6_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  ipv6_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }
  QuicChromiumClientSession* ipv4_session =
      GetPendingSession(kDefaultDestination);
  EXPECT_EQ(ToIPEndPoint(ipv4_session->peer_address()),
            MakeIPEndPoint(kIpv4Addr1));

  // The connectors change slots, so the connector created here takes IPv6.
  FastForwardBy(SlowTimerDelay());
  EXPECT_FALSE(callback_.have_result());
  EXPECT_EQ(crypto_client_stream_factory_.streams().size(), 1u);

  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->CallOnServiceEndpointsUpdated();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv6Addr1));

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  ipv6_data.ExpectAllReadDataConsumed();
  ipv6_data.ExpectAllWriteDataConsumed();

  EXPECT_FALSE(
      net_log_observer_
          .GetEntriesWithType(
              NetLogEventType::QUIC_SESSION_POOL_ASYNC_DNS_JOB_SLOTS_SWAPPED)
          .empty());

  // The slow timer created the successful connector. Moving it to the primary
  // slot does not change its source.
  histograms.ExpectUniqueSample(
      "Net.QuicSession.AsyncDnsJob.SuccessSource",
      static_cast<int>(SuccessSource::kSlowTimerConnector), 1);
  histograms.ExpectUniqueSample(
      "Net.QuicSession.AsyncDnsJob.AttemptsPerJob.JobSucceeded", 2, 1);
}

// A fixture with the slow timer disabled through its feature param. The param
// must be settled with the rest of the features, before the task environment
// threads run.
class QuicSessionPoolAsyncDnsJobZeroDelayTest
    : public QuicSessionPoolAsyncDnsJobTest {
 protected:
  QuicSessionPoolAsyncDnsJobZeroDelayTest()
      : QuicSessionPoolAsyncDnsJobTest(EnabledFeatures(),
                                       DisabledFeatures(),
                                       {{features::kAsyncDnsQuicJob, {}},
                                        {features::kAdjustQuicSlowTimerDelay,
                                         {{"QuicSlowTimerDelay", "0ms"}}}}) {}
};

INSTANTIATE_TEST_SUITE_P(All,
                         QuicSessionPoolAsyncDnsJobZeroDelayTest,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "AsyncQuicSession"
                                             : "SyncQuicSession";
                         });

// A slow timer delay of zero keeps the job on one connector. Time passing
// changes nothing, and the job still falls back to the next candidate when an
// attempt fails.
TEST_P(QuicSessionPoolAsyncDnsJobZeroDelayTest,
       ZeroSlowTimerDelayKeepsOneConnector) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData ipv6_data(version_);
  ipv6_data.AddReadPause();
  ipv6_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);
  ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData ipv4_data(version_);
  ipv4_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  ipv4_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }

  // No second connector appears, so the IPv4 candidate stays untried while
  // the IPv6 attempt runs. The delay param is zero in this fixture, so
  // advance past where the default delay would have fired the timer.
  FastForwardBy(features::kQuicSlowTimerDelay.default_value * 2);
  EXPECT_FALSE(callback_.have_result());
  EXPECT_EQ(crypto_client_stream_factory_.streams().size(), 1u);

  // The IPv6 attempt fails, so the connector falls back to IPv4 by itself.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  ipv6_data.Resume();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv4Addr1));

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  ipv4_data.ExpectAllReadDataConsumed();
  ipv4_data.ExpectAllWriteDataConsumed();
}

// Both connectors run out of candidates after resolution finished. The job
// fails with the result of the attempt that failed most recently, which the
// secondary connector ran, and not with the result of the primary connector.
TEST_P(QuicSessionPoolAsyncDnsJobTest, BothConnectorsExhaustAfterResolution) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData ipv6_data(version_);
  ipv6_data.AddReadPause();
  ipv6_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);
  ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData ipv4_data(version_);
  ipv4_data.AddReadPause();
  ipv4_data.AddRead(ASYNC, ERR_CONNECTION_REFUSED);
  ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  // The last attempt of the secondary connector fails while it starts.
  MockQuicData second_ipv4_data(version_);
  second_ipv4_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_INVALID);
  second_ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1, kIpv4Addr2));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }

  FastForwardBy(SlowTimerDelay());
  EXPECT_EQ(crypto_client_stream_factory_.streams().size(), 2u);

  // The primary connector runs out of IPv6 candidates first. Its attempt
  // fails while its session is in the crypto handshake.
  ipv6_data.Resume();
  EXPECT_FALSE(callback_.have_result());

  // The secondary connector then runs out of IPv4 candidates, and its last
  // attempt fails with a different result.
  ipv4_data.Resume();
  EXPECT_FALSE(callback_.have_result());

  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_ADDRESS_INVALID));
  EXPECT_FALSE(HasActiveSession(kDefaultDestination));
}

// The attempt that failed most recently belongs to the primary connector,
// which reported running out of candidates before that failure. The job
// reports that failure and not the result of the connector that reported
// last.
TEST_P(QuicSessionPoolAsyncDnsJobTest, JobFailsWithMostRecentAttemptFailure) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  base::HistogramTester histograms;
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData ipv6_data(version_);
  ipv6_data.AddReadPause();
  ipv6_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);
  ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData ipv4_data(version_);
  ipv4_data.AddReadPause();
  ipv4_data.AddRead(ASYNC, ERR_CONNECTION_REFUSED);
  ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  // The attempt to the late IPv6 candidate fails while it starts, so the
  // primary connector reports nothing about it.
  MockQuicData late_ipv6_data(version_);
  late_ipv6_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_INVALID);
  late_ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }

  FastForwardBy(SlowTimerDelay());
  EXPECT_EQ(crypto_client_stream_factory_.streams().size(), 2u);

  // The primary connector reports running out of candidates first, then the
  // secondary connector does.
  ipv6_data.Resume();
  ipv4_data.Resume();
  EXPECT_FALSE(callback_.have_result());

  // A late IPv6 candidate gives the primary connector one more attempt, which
  // fails after both connectors already reported.
  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr2));
  endpoint_request->CallOnServiceEndpointsUpdated();
  EXPECT_FALSE(callback_.have_result());

  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_ADDRESS_INVALID));
  EXPECT_FALSE(HasActiveSession(kDefaultDestination));

  histograms.ExpectUniqueSample(
      "Net.QuicSession.AsyncDnsJob.AttemptsPerJob.JobFailed", 3, 1);
  histograms.ExpectTimeBucketCount("Net.QuicSession.AsyncDnsJob.TimeToFailure",
                                   SlowTimerDelay(), 1);
  histograms.ExpectTotalCount("Net.QuicSession.AsyncDnsJob.SuccessSource", 0);
}

// The job completes before the slow timer fires. Time passing afterwards
// starts nothing.
TEST_P(QuicSessionPoolAsyncDnsJobTest, TimeAfterJobCompletionDoesNothing) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv6Addr1));

  // Only one socket is available, so a second attempt would fail this test.
  FastForwardBy(SlowTimerDelay() * 2);
  EXPECT_EQ(crypto_client_stream_factory_.streams().size(), 1u);
  EXPECT_TRUE(HasActiveSession(kDefaultDestination));

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// Once both slots are filled, a late IPv4 candidate is attempted by the
// secondary connector, and the secondary connector leaves a visible IPv6
// candidate to the primary connector.
TEST_P(QuicSessionPoolAsyncDnsJobTest, SlotsAreExclusiveAfterSplit) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData ipv6_data(version_);
  ipv6_data.AddReadPause();
  ipv6_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);
  ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData ipv4_data(version_);
  ipv4_data.AddReadPause();
  ipv4_data.AddRead(ASYNC, ERR_CONNECTION_REFUSED);
  ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  // The socket of the next attempt the secondary connector starts.
  MockQuicData second_ipv4_data(version_);
  second_ipv4_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  second_ipv4_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  second_ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }

  // The secondary connector has no IPv4 candidate yet.
  FastForwardBy(SlowTimerDelay());
  EXPECT_EQ(crypto_client_stream_factory_.streams().size(), 1u);

  // The primary connector keeps its attempt, so only the secondary connector
  // can take the IPv4 candidate that arrives here.
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->CallOnServiceEndpointsUpdated();
  crypto_client_stream_factory_.WaitForStreams(2);

  // An IPv6 and an IPv4 candidate arrive together, and an attempt to either
  // of them would succeed right away.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr2));
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr2));
  endpoint_request->CallOnServiceEndpointsUpdated();

  // The secondary connector takes the IPv4 candidate and leaves the IPv6 one
  // to the primary connector.
  ipv4_data.Resume();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv4Addr2));

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  second_ipv4_data.ExpectAllReadDataConsumed();
  second_ipv4_data.ExpectAllWriteDataConsumed();
}

// The primary connector's session creation fails while the secondary
// connector is racing it. The failure is held, the successful creation of the
// secondary connector reaches the requests instead, and a request that
// arrives afterwards is not promised the signal a second time.
TEST_P(QuicSessionPoolAsyncDnsJobTest, SessionCreationSignalRace) {
  if (!async_quic_session()) {
    // Requests wait for the session creation signal only when session
    // creation is asynchronous.
    GTEST_SKIP();
  }

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // The first attempt of the primary connector fails to create its session.
  MockQuicData first_ipv6_data(version_);
  first_ipv6_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  first_ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  // The next attempt of the primary connector never finishes creating its
  // session. The completer keeps the connect pending; a pending connect
  // result instead would leak the socket, which is owned by the connect
  // callback the mock socket itself holds.
  MockConnectCompleter second_ipv6_connect_completer;
  MockQuicData second_ipv6_data(version_);
  second_ipv6_data.AddConnect(&second_ipv6_connect_completer);
  second_ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData ipv4_data(version_);
  ipv4_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  ipv4_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  EXPECT_TRUE(
      builder.request.WaitForQuicSessionCreation(creation_callback.callback()));

  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr2));
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  // The failed creation of the primary connector is held, so the requests
  // hear nothing until the secondary connector created its session.
  EXPECT_FALSE(creation_callback.have_result());

  FastForwardBy(SlowTimerDelay());
  EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));

  // The primary connector is still creating a session, but the signal has
  // already fired and never fires twice.
  RequestBuilder builder2(this);
  TestCompletionCallback callback2;
  builder2.callback = callback2.callback();
  EXPECT_THAT(builder2.CallRequest(), IsError(ERR_IO_PENDING));
  TestCompletionCallback stale_callback;
  EXPECT_FALSE(
      builder2.request.WaitForQuicSessionCreation(stale_callback.callback()));

  // Finish the handshake of the attempt the secondary connector started.
  ASSERT_EQ(crypto_client_stream_factory_.streams().size(), 1u);
  crypto_client_stream_factory_.streams()[0]->NotifySessionZeroRttComplete();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv4Addr1));

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&builder2.request);
  EXPECT_TRUE(stream2.get());

  ipv4_data.ExpectAllReadDataConsumed();
  ipv4_data.ExpectAllWriteDataConsumed();

  EXPECT_FALSE(
      net_log_observer_
          .GetEntriesWithType(
              NetLogEventType::
                  QUIC_SESSION_POOL_ASYNC_DNS_JOB_SESSION_CREATION_HELD)
          .empty());
}

// The job settles on the other connector while the discarded attempt's
// session is mid handshake. Cancelling the attempt closes that session right
// away instead of leaving it behind until its handshake times out.
TEST_P(QuicSessionPoolAsyncDnsJobTest, DiscardedSessionClosedOnSettle) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  // The discarded session sends its first handshake packet and hears nothing
  // back. The close is silent, so the socket sees no other packet.
  MockQuicData ipv6_data(version_);
  ipv6_data.AddReadPauseForever();
  ipv6_data.AddWrite(SYNCHRONOUS, ERR_IO_PENDING);
  ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData ipv4_data(version_);
  ipv4_data.AddReadPauseForever();
  ipv4_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }
  QuicChromiumClientSession* ipv6_session =
      GetPendingSession(kDefaultDestination);
  EXPECT_EQ(ToIPEndPoint(ipv6_session->peer_address()),
            MakeIPEndPoint(kIpv6Addr1));

  // The attempt the secondary connector starts finishes its handshake right
  // away, so the job settles while the discarded session is mid handshake.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::CONFIRM_HANDSHAKE);
  FastForwardBy(SlowTimerDelay());
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv4Addr1));

  // No clock moved past any timeout. The discarded session is gone because
  // the cancelled attempt closed it.
  EXPECT_FALSE(QuicSessionPoolPeer::IsLiveSession(pool_.get(), ipv6_session));
  EXPECT_TRUE(HasActiveSession(kDefaultDestination));

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());
}

// After DNS resolution completes, connectors may cross over to attempt the
// other family's remaining candidates if their preferred family is exhausted.
// Here the primary connector exhausts IPv6 candidates after DNS finishes and
// attempts the remaining untried IPv4 candidate, succeeding.
TEST_P(QuicSessionPoolAsyncDnsJobTest,
       PrimaryConnectorCrossesOverToIPv4AfterDnsCompletes) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // The primary connector's first IPv6 attempt fails when resumed.
  MockQuicData ipv6_data(version_);
  ipv6_data.AddReadPause();
  ipv6_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);
  ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  // The secondary connector's IPv4 attempt never completes.
  MockQuicData first_ipv4_data(version_);
  first_ipv4_data.AddReadPauseForever();
  first_ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  // The primary connector's crossover attempt to the second IPv4 candidate
  // succeeds immediately.
  MockQuicData second_ipv4_data(version_);
  second_ipv4_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  second_ipv4_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  second_ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1, kIpv4Addr2));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  // Slow timer fires and secondary connector starts the first IPv4 candidate.
  FastForwardBy(SlowTimerDelay());
  ASSERT_EQ(crypto_client_stream_factory_.streams().size(), 2u);

  // DNS resolution finishes.
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  // The IPv6 attempt fails. Because DNS is finished and IPv6 is exhausted,
  // the primary connector crosses over and takes the untried second IPv4
  // candidate.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  ipv6_data.Resume();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv4Addr2));

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  second_ipv4_data.ExpectAllReadDataConsumed();
  second_ipv4_data.ExpectAllWriteDataConsumed();
}

// The secondary connector exhausts IPv4 candidates after DNS resolution
// finishes and crosses over to attempt an untried IPv6 candidate, succeeding.
TEST_P(QuicSessionPoolAsyncDnsJobTest,
       SecondaryConnectorCrossesOverToIPv6AfterDnsCompletes) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  base::HistogramTester histograms;
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // The primary connector's IPv6 attempt to kIpv6Addr1 never completes.
  MockQuicData first_ipv6_data(version_);
  first_ipv6_data.AddReadPauseForever();
  first_ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  // The secondary connector's first IPv4 attempt fails when resumed.
  MockQuicData ipv4_data(version_);
  ipv4_data.AddReadPause();
  ipv4_data.AddRead(ASYNC, ERR_CONNECTION_REFUSED);
  ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  // The secondary connector's crossover attempt to the second IPv6 candidate
  // succeeds immediately.
  MockQuicData second_ipv6_data(version_);
  second_ipv6_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  second_ipv6_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  second_ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  endpoint_request->add_endpoint(ServiceEndpointBuilder()
                                     .add_v6(kIpv6Addr1, kDefaultServerPort)
                                     .add_v6(kIpv6Addr2, kDefaultServerPort)
                                     .endpoint());
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  // Slow timer fires and secondary connector starts IPv4 attempt.
  FastForwardBy(SlowTimerDelay());
  ASSERT_EQ(crypto_client_stream_factory_.streams().size(), 2u);

  // DNS resolution finishes.
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  // The IPv4 attempt fails. Because DNS is finished and IPv4 is exhausted,
  // the secondary connector crosses over and takes the untried second IPv6
  // candidate.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  ipv4_data.Resume();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv6Addr2));

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  second_ipv6_data.ExpectAllReadDataConsumed();
  second_ipv6_data.ExpectAllWriteDataConsumed();

  // This is a later attempt, but it still belongs to the connector created by
  // the slow timer.
  histograms.ExpectUniqueSample(
      "Net.QuicSession.AsyncDnsJob.SuccessSource",
      static_cast<int>(SuccessSource::kSlowTimerConnector), 1);
}

// While DNS resolution is in flight, connectors do not cross over to the
// other family's candidates even if their preferred family is exhausted.
TEST_P(QuicSessionPoolAsyncDnsJobTest, NoCrossoverWhileDnsResolutionInFlight) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // The primary connector's IPv6 attempt fails when resumed.
  MockQuicData ipv6_data(version_);
  ipv6_data.AddReadPause();
  ipv6_data.AddRead(ASYNC, ERR_ADDRESS_UNREACHABLE);
  ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  // The secondary connector's first IPv4 attempt never completes.
  MockQuicData first_ipv4_data(version_);
  first_ipv4_data.AddReadPauseForever();
  first_ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  // The primary connector's crossover attempt once DNS finishes.
  MockQuicData second_ipv4_data(version_);
  second_ipv4_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  second_ipv4_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  second_ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1, kIpv4Addr2));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  // Slow timer fires and secondary connector starts IPv4 attempt.
  FastForwardBy(SlowTimerDelay());
  ASSERT_EQ(crypto_client_stream_factory_.streams().size(), 2u);

  // The IPv6 attempt fails while DNS is still in flight.
  ipv6_data.Resume();

  // The primary connector must NOT take kIpv4Addr2 yet because resolution is
  // still in flight. No 3rd stream is created.
  EXPECT_EQ(crypto_client_stream_factory_.streams().size(), 2u);
  EXPECT_FALSE(callback_.have_result());

  // Once DNS resolution completes, the primary connector crosses over to the
  // untried IPv4 candidate.
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ZERO_RTT);
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv4Addr2));

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  second_ipv4_data.ExpectAllReadDataConsumed();
  second_ipv4_data.ExpectAllWriteDataConsumed();
}

}  // namespace

// Verifies that when an existing session becomes active during DNS resolution,
// the job succeeds with SuccessSource::kActiveSession.
TEST_P(QuicSessionPoolAsyncDnsJobTest, ActiveSessionPooledDuringDnsResolution) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request1 =
      fake_resolver_.AddFakeRequest();
  endpoint_request1->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request1->CompleteStartAsynchronously(OK);
  InitializeWithFakeResolver();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request2 =
      fake_resolver_.AddFakeRequest();
  const url::SchemeHostPort server2(url::kHttpsScheme, kServer2HostName,
                                    kDefaultServerPort);

  base::HistogramTester histograms;
  RequestBuilder builder2(this);
  builder2.destination = server2;
  builder2.url = GURL(kServer2Url);
  TestCompletionCallback callback2;
  builder2.callback = callback2.callback();
  EXPECT_THAT(builder2.CallRequest(), IsError(ERR_IO_PENDING));

  quic::OriginFrame frame;
  frame.origins.push_back(base::StrCat({"https://", kServer2HostName}));
  GetActiveSession(kDefaultDestination)->OnOriginFrame(frame);
  ASSERT_EQ(1u,
            GetActiveSession(kDefaultDestination)->received_origins().size());

  test::QuicSessionPoolPeer::ActivateAndMapSessionToAliasKey(
      pool_.get(), QuicSessionAliasKey(server2, builder2.request.session_key()),
      GetActiveSession(kDefaultDestination));

  endpoint_request2->add_endpoint(MakeUsableEndpoint("192.168.0.2"));
  endpoint_request2->set_crypto_ready(true);
  endpoint_request2->CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(callback2.have_result());
  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  EXPECT_NE(pool_->FindExistingSession(builder2.request.session_key(), server2),
            nullptr);

  std::unique_ptr<HttpStream> stream2 = CreateStream(&builder2.request);
  EXPECT_TRUE(stream2.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();

  histograms.ExpectUniqueSample("Net.QuicSession.AsyncDnsJob.SuccessSource",
                                SuccessSource::kActiveSession, 1);
}

// Verifies that endpoints with supported QUIC ALPN but without IP addresses
// are skipped in GetUsableEndpoints.
TEST_P(QuicSessionPoolAsyncDnsJobTest, SkipsEndpointsWithoutIpAddresses) {
  quic_params_->supported_versions = {version_};
  std::vector<HostResolverEndpointResult> endpoints(2);
  endpoints[0].ip_endpoints = {};
  endpoints[0].metadata.supported_protocol_alpns = {
      quic::AlpnForVersion(version_)};

  endpoints[1].ip_endpoints = {IPEndPoint(IPAddress(192, 0, 2, 2), 443)};
  endpoints[1].metadata.supported_protocol_alpns = {
      quic::AlpnForVersion(version_)};

  host_resolver_->rules()->AddRule(
      kDefaultServerHostName,
      MockHostResolverBase::RuleResolver::RuleResult(
          std::move(endpoints),
          /*aliases=*/std::set<std::string>{kDefaultServerHostName}));

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  builder.quic_version = quic::ParsedQuicVersion::Unsupported();
  builder.require_dns_https_alpn = true;

  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(
      kDefaultDestination, PRIVACY_MODE_DISABLED, NetworkAnonymizationKey(),
      ProxyChain::Direct(), SessionUsage::kDestination,
      /*require_dns_https_alpn=*/true);
  ASSERT_TRUE(session);
  IPEndPoint peer_address;
  EXPECT_THAT(session->GetDefaultSocket()->GetPeerAddress(&peer_address),
              IsOk());
  EXPECT_EQ(peer_address, IPEndPoint(IPAddress(192, 0, 2, 2), 443));

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// Verifies that a job fails when all service endpoints lack IP addresses.
TEST_P(QuicSessionPoolAsyncDnsJobTest, FailsWhenEndpointsLackIpAddresses) {
  quic_params_->supported_versions = {version_};
  std::vector<HostResolverEndpointResult> endpoints(1);
  endpoints[0].ip_endpoints = {};
  endpoints[0].metadata.supported_protocol_alpns = {
      quic::AlpnForVersion(version_)};

  host_resolver_->rules()->AddRule(
      kDefaultServerHostName,
      MockHostResolverBase::RuleResolver::RuleResult(
          std::move(endpoints),
          /*aliases=*/std::set<std::string>{kDefaultServerHostName}));

  Initialize();

  RequestBuilder builder(this);
  builder.quic_version = quic::ParsedQuicVersion::Unsupported();
  builder.require_dns_https_alpn = true;

  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback_.WaitForResult(),
              IsError(ERR_DNS_NO_MATCHING_SUPPORTED_ALPN));
}

// Verifies that SVCB records are considered optional when ECH is globally
// disabled.
TEST_P(QuicSessionPoolAsyncDnsJobTest, SvcbOptionalWhenEchDisabled) {
  quic_params_->supported_versions = {version_};
  HostResolverEndpointResult endpoint;
  endpoint.ip_endpoints = {IPEndPoint(IPAddress(192, 0, 2, 1), 443)};
  endpoint.metadata.ech_config_list = {1, 2, 3, 4};

  host_resolver_->rules()->AddRule(
      kDefaultServerHostName,
      MockHostResolverBase::RuleResolver::RuleResult(
          {endpoint},
          /*aliases=*/std::set<std::string>{kDefaultServerHostName}));

  ssl_config_service_.SetEchModeGetter(
      std::make_unique<TestStaticEchModeGetter>(EchMode::kDisabled,
                                                kDefaultServerHostName));

  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  builder.quic_version = version_;

  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  QuicChromiumClientSession* session = GetActiveSession(kDefaultDestination);
  ASSERT_TRUE(session);
  IPEndPoint peer_address;
  EXPECT_THAT(session->GetDefaultSocket()->GetPeerAddress(&peer_address),
              IsOk());
  EXPECT_EQ(peer_address, IPEndPoint(IPAddress(192, 0, 2, 1), 443));

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// Verifies that updating a job's priority to its current priority does not
// call the host resolver's ChangeRequestPriority.
TEST_P(QuicSessionPoolAsyncDnsJobTest, UpdatePriorityToSamePriority) {
  host_resolver_->set_ondemand_mode(true);
  Initialize();
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  builder.priority = DEFAULT_PRIORITY;
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  const size_t resolver_request_id = host_resolver_->last_id();
  EXPECT_EQ(DEFAULT_PRIORITY,
            host_resolver_->request_priority(resolver_request_id));
  EXPECT_EQ(0u, host_resolver_->num_change_request_priority_calls(
                    resolver_request_id));

  // Setting the priority to the same priority should not call
  // ChangeRequestPriority().
  builder.request.SetPriority(DEFAULT_PRIORITY);
  EXPECT_EQ(DEFAULT_PRIORITY,
            host_resolver_->request_priority(resolver_request_id));
  EXPECT_EQ(0u, host_resolver_->num_change_request_priority_calls(
                    resolver_request_id));

  // Changing priority to a new value should call ChangeRequestPriority().
  builder.request.SetPriority(HIGHEST);
  EXPECT_EQ(HIGHEST, host_resolver_->request_priority(resolver_request_id));
  EXPECT_EQ(1u, host_resolver_->num_change_request_priority_calls(
                    resolver_request_id));

  // Setting the priority again to HIGHEST should not call
  // ChangeRequestPriority().
  builder.request.SetPriority(HIGHEST);
  EXPECT_EQ(HIGHEST, host_resolver_->request_priority(resolver_request_id));
  EXPECT_EQ(1u, host_resolver_->num_change_request_priority_calls(
                    resolver_request_id));

  host_resolver_->ResolveAllPending();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// Verifies that ProcessServiceEndpointResults() returns ERR_IO_PENDING when
// service endpoints become empty while an attempt is in flight.
TEST_P(QuicSessionPoolAsyncDnsJobTest,
       EndpointsBecomeEmptyWhileAttemptInFlight) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  endpoint_request->set_endpoints({});
  endpoint_request->CallOnServiceEndpointsUpdated();

  EXPECT_FALSE(callback_.have_result());

  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  EXPECT_FALSE(callback_.have_result());

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }
  crypto_client_stream_factory_.last_stream()->NotifySessionZeroRttComplete();
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
}

// Verifies that AddRequest() subscribes to session creation notifications when
// the secondary connector is awaiting session creation.
TEST_P(QuicSessionPoolAsyncDnsJobTest,
       AddRequestWhileSecondaryConnectorAwaitingSessionCreation) {
  if (!async_quic_session()) {
    // Requests wait for the session creation signal only when session
    // creation is asynchronous.
    GTEST_SKIP();
  }

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData ipv6_data(version_);
  ipv6_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  MockConnectCompleter ipv4_connect_completer;
  MockQuicData ipv4_data(version_);
  ipv4_data.AddConnect(&ipv4_connect_completer);
  ipv4_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  ipv4_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder1(this);
  EXPECT_THAT(builder1.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback1;
  EXPECT_TRUE(builder1.request.WaitForQuicSessionCreation(
      creation_callback1.callback()));

  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  EXPECT_FALSE(creation_callback1.have_result());

  FastForwardBy(SlowTimerDelay());
  EXPECT_EQ(crypto_client_stream_factory_.streams().size(), 0u);

  // The secondary connector is awaiting session creation on ipv4_data.
  RequestBuilder builder2(this);
  TestCompletionCallback callback2;
  builder2.callback = callback2.callback();
  EXPECT_THAT(builder2.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback2;
  EXPECT_TRUE(builder2.request.WaitForQuicSessionCreation(
      creation_callback2.callback()));

  ipv4_connect_completer.Complete(OK);
  EXPECT_THAT(creation_callback1.WaitForResult(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(creation_callback2.WaitForResult(), IsError(ERR_IO_PENDING));
  EXPECT_EQ(crypto_client_stream_factory_.streams().size(), 1u);

  crypto_client_stream_factory_.last_stream()->NotifySessionZeroRttComplete();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(crypto_client_stream_factory_.streams().size(), 1u);
  EXPECT_TRUE(callback2.have_result());
  EXPECT_THAT(callback2.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream1 = CreateStream(&builder1.request);
  EXPECT_TRUE(stream1.get());
  std::unique_ptr<HttpStream> stream2 = CreateStream(&builder2.request);
  EXPECT_TRUE(stream2.get());

  ipv4_data.ExpectAllReadDataConsumed();
  ipv4_data.ExpectAllWriteDataConsumed();
}

// Verifies that connection failures on the default network before handshake are
// forwarded to requests.
TEST_P(QuicSessionPoolAsyncDnsJobTest, OnConnectionFailedOnDefaultNetwork) {
  if (async_quic_session()) {
    GTEST_SKIP();
  }
  quic_params_->retry_on_alternate_network_before_handshake = true;
  quic_params_->migrate_sessions_on_network_change_v2 = true;
  quic_params_->migrate_sessions_early_v2 = true;
  scoped_mock_network_change_notifier_ =
      std::make_unique<ScopedMockNetworkChangeNotifier>();
  MockNetworkChangeNotifier* mock_ncn =
      scoped_mock_network_change_notifier_->mock_network_change_notifier();
  mock_ncn->ForceNetworkHandlesSupported();
  mock_ncn->SetConnectedNetworksList(
      {kDefaultNetworkForTests, kNewNetworkForTests});

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::COLD_START_WITH_CHLO_SENT);

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddWrite(SYNCHRONOUS, ERR_CONNECTION_RESET);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData socket_data2(version_);
  socket_data2.AddReadPauseForever();
  socket_data2.AddWrite(SYNCHRONOUS, ERR_CONNECTION_RESET);
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  endpoint_request->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  EXPECT_TRUE(failed_on_default_network_);

  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_QUIC_HANDSHAKE_FAILED));

  socket_data.ExpectAllReadDataConsumed();
  socket_data.ExpectAllWriteDataConsumed();
  socket_data2.ExpectAllReadDataConsumed();
  socket_data2.ExpectAllWriteDataConsumed();
}

class QuicSessionPoolAsyncDnsJobRTTBasedTest
    : public QuicSessionPoolAsyncDnsJobTest {
 public:
  static std::vector<base::test::FeatureRef> Disabled() {
    auto disabled = DisabledFeatures();
    disabled.push_back(features::kAdjustQuicSlowTimerDelay);
    return disabled;
  }

  QuicSessionPoolAsyncDnsJobRTTBasedTest()
      : QuicSessionPoolAsyncDnsJobTest(EnabledFeatures(),
                                       Disabled(),
                                       {{features::kQuicSlowTimerBasedOnRTT,
                                         {{"QuicSlowTimerRTTMultiplier", "2.0"},
                                          {"QuicSlowTimerMin", "10ms"},
                                          {"QuicSlowTimerMax", "1s"}}},
                                        {features::kAsyncDnsQuicJob, {}}}) {}

  static constexpr base::TimeDelta kRTT = base::Milliseconds(50);

  void SetUp() override {
    QuicSessionPoolAsyncDnsJobTest::SetUp();

    // Set up HttpServerProperties with a specific RTT.
    url::SchemeHostPort server(url::kHttpsScheme, kDefaultServerHostName, 443);
    ServerNetworkStats stats;
    stats.srtt = kRTT;
    http_server_properties_->SetServerNetworkStats(
        server, NetworkAnonymizationKey(), stats);
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         QuicSessionPoolAsyncDnsJobRTTBasedTest,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "AsyncQuicSession"
                                             : "SyncQuicSession";
                         });

TEST_P(QuicSessionPoolAsyncDnsJobRTTBasedTest, UsesRTTForSlowTimer) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // The IPv6 attempt never finishes its handshake.
  MockQuicData ipv6_data(version_);
  ipv6_data.AddReadPauseForever();
  ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData ipv4_data(version_);
  ipv4_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  ipv4_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }

  const double kRTTMultiplier = 2.0;
  const base::TimeDelta kExpectedDelay =
      QuicSessionPoolAsyncDnsJobRTTBasedTest::kRTT * kRTTMultiplier;

  // Wait for just before the timer fires.
  FastForwardBy(kExpectedDelay - base::Milliseconds(1));
  EXPECT_FALSE(callback_.have_result());
  ASSERT_EQ(crypto_client_stream_factory_.streams().size(), 1u);

  // Timer fires and secondary connector starts IPv4 attempt.
  FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(callback_.have_result());
  ASSERT_EQ(crypto_client_stream_factory_.streams().size(), 2u);

  crypto_client_stream_factory_.streams()[1]->NotifySessionZeroRttComplete();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  ipv6_data.ExpectAllReadDataConsumed();
  ipv6_data.ExpectAllWriteDataConsumed();

  ipv4_data.ExpectAllReadDataConsumed();
  ipv4_data.ExpectAllWriteDataConsumed();
}

TEST_P(QuicSessionPoolAsyncDnsJobRTTBasedTest, UsesNqeRTTForSlowTimer) {
  // Clear the ServerNetworkStats RTT set in SetUp().
  url::SchemeHostPort server(url::kHttpsScheme, kDefaultServerHostName, 443);
  http_server_properties_->ClearServerNetworkStats(server,
                                                   NetworkAnonymizationKey());

  // Set up NQE with a specific RTT.
  test_network_quality_estimator_ =
      std::make_unique<TestNetworkQualityEstimator>();
  const base::TimeDelta kNqeRTT = base::Milliseconds(100);
  test_network_quality_estimator_->SetStartTimeNullTransportRtt(kNqeRTT);

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // The IPv6 attempt never finishes its handshake.
  MockQuicData ipv6_data(version_);
  ipv6_data.AddReadPauseForever();
  ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData ipv4_data(version_);
  ipv4_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  ipv4_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }

  const double kRTTMultiplier = 2.0;
  const base::TimeDelta kExpectedDelay = kNqeRTT * kRTTMultiplier;

  // Wait for just before the timer fires.
  FastForwardBy(kExpectedDelay - base::Milliseconds(1));
  EXPECT_FALSE(callback_.have_result());
  ASSERT_EQ(crypto_client_stream_factory_.streams().size(), 1u);

  // Timer fires and secondary connector starts IPv4 attempt.
  FastForwardBy(base::Milliseconds(1));
  ASSERT_EQ(crypto_client_stream_factory_.streams().size(), 2u);

  crypto_client_stream_factory_.streams()[1]->NotifySessionZeroRttComplete();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  ipv6_data.ExpectAllReadDataConsumed();
  ipv6_data.ExpectAllWriteDataConsumed();

  ipv4_data.ExpectAllReadDataConsumed();
  ipv4_data.ExpectAllWriteDataConsumed();
}

class QuicSessionPoolAsyncDnsJobStaticTimerTest
    : public QuicSessionPoolAsyncDnsJobTest {
 public:
  static constexpr base::TimeDelta kStaticDelay = base::Milliseconds(250);

  static std::vector<base::test::FeatureRef> Disabled() {
    auto disabled = DisabledFeatures();
    disabled.push_back(features::kQuicSlowTimerBasedOnRTT);
    return disabled;
  }

  QuicSessionPoolAsyncDnsJobStaticTimerTest()
      : QuicSessionPoolAsyncDnsJobTest(EnabledFeatures(),
                                       Disabled(),
                                       {{features::kAdjustQuicSlowTimerDelay,
                                         {{"QuicSlowTimerDelay", "250ms"}}},
                                        {features::kAsyncDnsQuicJob, {}}}) {}
};

INSTANTIATE_TEST_SUITE_P(All,
                         QuicSessionPoolAsyncDnsJobStaticTimerTest,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "AsyncQuicSession"
                                             : "SyncQuicSession";
                         });

TEST_P(QuicSessionPoolAsyncDnsJobStaticTimerTest, UsesStaticTimer) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);

  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // The IPv6 attempt never finishes its handshake.
  MockQuicData ipv6_data(version_);
  ipv6_data.AddReadPauseForever();
  ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData ipv4_data(version_);
  ipv4_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  ipv4_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        creation_callback.callback()));
  }

  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_IO_PENDING));
  }

  // Wait for just before the timer fires.
  FastForwardBy(QuicSessionPoolAsyncDnsJobStaticTimerTest::kStaticDelay -
                base::Milliseconds(1));
  EXPECT_FALSE(callback_.have_result());
  ASSERT_EQ(crypto_client_stream_factory_.streams().size(), 1u);

  // Timer fires and secondary connector starts IPv4 attempt.
  FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(callback_.have_result());
  ASSERT_EQ(crypto_client_stream_factory_.streams().size(), 2u);

  crypto_client_stream_factory_.streams()[1]->NotifySessionZeroRttComplete();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  ipv4_data.ExpectAllReadDataConsumed();
  ipv4_data.ExpectAllWriteDataConsumed();
}

// Verifies that destroying the pool while an attempt is in flight with
// multiple endpoints does not advance to next candidates and safely cleans up.
TEST_P(QuicSessionPoolAsyncDnsJobTest,
       DestroyPoolWhileAttemptInFlightWithMultipleEndpoints) {
  host_resolver_->set_synchronous_mode(true);
  host_resolver_->rules()->AddIPLiteralRule(kDefaultServerHostName,
                                            "192.168.0.1,192.168.0.2", "");
  Initialize();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  MockQuicData socket_data1(version_);
  socket_data1.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_INITIAL);
  socket_data1.AddWrite(
      SYNCHRONOUS,
      client_maker_.Packet(1)
          .AddConnectionCloseFrame(quic::QUIC_CONNECTION_CANCELLED, "net error")
          .AddPaddingFrame()
          .Build());
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData socket_data2(version_);
  socket_data2.AddReadPauseForever();
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  {
    RequestBuilder builder(this);
    EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));
  }

  pool_.reset();
}

class QuicSessionPoolAsyncDnsJobFastFailTest
    : public QuicSessionPoolAsyncDnsJobTest {
 protected:
  QuicSessionPoolAsyncDnsJobFastFailTest()
      : QuicSessionPoolAsyncDnsJobTest(
            EnabledFeatures(),
            DisabledFeatures(),
            {{features::kAsyncDnsQuicJob,
              {{"AsyncDnsQuicJobFastFail", "true"}}}}) {}
};

INSTANTIATE_TEST_SUITE_P(All,
                         QuicSessionPoolAsyncDnsJobFastFailTest,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "AsyncQuicSession"
                                             : "SyncQuicSession";
                         });

TEST_P(QuicSessionPoolAsyncDnsJobFastFailTest, SessionCreationSignalFastFail) {
  if (!async_quic_session()) {
    // Requests wait for the session creation signal only when session
    // creation is asynchronous.
    GTEST_SKIP();
  }

  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // The first attempt of the primary connector fails to create its session.
  MockQuicData first_ipv6_data(version_);
  first_ipv6_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_IN_USE);
  first_ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  // The next attempt of the primary connector never finishes creating its
  // session.
  MockConnectCompleter second_ipv6_connect_completer;
  MockQuicData second_ipv6_data(version_);
  second_ipv6_data.AddConnect(&second_ipv6_connect_completer);
  second_ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData ipv4_data(version_);
  ipv4_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  ipv4_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback creation_callback;
  EXPECT_TRUE(
      builder.request.WaitForQuicSessionCreation(creation_callback.callback()));

  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr2));
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  // With fast-fail enabled, the failed session creation of the primary
  // connector is notified immediately instead of being held.
  EXPECT_THAT(creation_callback.WaitForResult(), IsError(ERR_ADDRESS_IN_USE));

  FastForwardBy(SlowTimerDelay());

  // Finish the handshake of the attempt the secondary connector started.
  ASSERT_EQ(crypto_client_stream_factory_.streams().size(), 1u);
  crypto_client_stream_factory_.streams()[0]->NotifySessionZeroRttComplete();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv4Addr1));

  std::unique_ptr<HttpStream> stream = CreateStream(&builder.request);
  EXPECT_TRUE(stream.get());

  ipv4_data.ExpectAllReadDataConsumed();
  ipv4_data.ExpectAllWriteDataConsumed();

  EXPECT_TRUE(net_log_observer_
                  .GetEntriesWithType(
                      NetLogEventType::
                          QUIC_SESSION_POOL_ASYNC_DNS_JOB_SESSION_CREATION_HELD)
                  .empty());
  EXPECT_FALSE(
      net_log_observer_
          .GetEntriesWithType(
              NetLogEventType::
                  QUIC_SESSION_POOL_ASYNC_DNS_JOB_SESSION_CREATION_SIGNALED)
          .empty());
}

class QuicSessionPoolAsyncDnsJobOptimisticDnsTest
    : public QuicSessionPoolAsyncDnsJobTest {
 protected:
  QuicSessionPoolAsyncDnsJobOptimisticDnsTest()
      : QuicSessionPoolAsyncDnsJobTest(
            EnabledFeatures({features::kOptimisticDnsForQuic}),
            DisabledFeatures(),
            {{features::kAsyncDnsQuicJob, {}}}) {}
};

INSTANTIATE_TEST_SUITE_P(All,
                         QuicSessionPoolAsyncDnsJobOptimisticDnsTest,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "AsyncQuicSession"
                                             : "SyncQuicSession";
                         });

TEST_P(QuicSessionPoolAsyncDnsJobOptimisticDnsTest, StaleFailsFreshSucceeds) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // Stale connection attempt will fail at the network level.
  MockQuicData stale_data(version_);
  stale_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  stale_data.AddSocketDataToFactory(socket_factory_.get());

  // Fresh connection attempt will succeed.
  MockQuicData fresh_data(version_);
  fresh_data.AddConnect(SYNCHRONOUS, OK);
  fresh_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  fresh_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  fresh_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  // Resolver provides stale result.
  endpoint_request->set_is_stale_while_refreshing(true);
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  // The job should NOT fail yet, because fresh DNS hasn't arrived.
  EXPECT_FALSE(callback_.have_result());
  // The stale attempt fails on connect because of ERR_ADDRESS_UNREACHABLE.
  endpoint_request->set_is_stale_while_refreshing(false);
  endpoint_request->set_endpoints({MakeUsableEndpoint(kIpv4Addr2)});

  TestCompletionCallback fresh_creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        fresh_creation_callback.callback()));
    EXPECT_FALSE(fresh_creation_callback.have_result());
  }

  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  if (async_quic_session()) {
    EXPECT_THAT(fresh_creation_callback.WaitForResult(),
                IsError(ERR_IO_PENDING));
  }

  // The fresh attempt starts.
  QuicChromiumClientSession* fresh_session =
      GetPendingSession(kDefaultDestination);
  EXPECT_EQ(ToIPEndPoint(fresh_session->peer_address()),
            MakeIPEndPoint(kIpv4Addr2));

  // The fresh attempt succeeds.
  crypto_client_stream_factory_.streams()[0]->NotifySessionZeroRttComplete();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv4Addr2));

  stale_data.ExpectAllReadDataConsumed();
  stale_data.ExpectAllWriteDataConsumed();
  fresh_data.ExpectAllReadDataConsumed();
  fresh_data.ExpectAllWriteDataConsumed();
}

TEST_P(QuicSessionPoolAsyncDnsJobOptimisticDnsTest, StaleSucceeds) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::CONFIRM_HANDSHAKE);

  // Stale connection attempt will succeed.
  MockQuicData stale_data(version_);
  stale_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  stale_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  stale_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  // Resolver provides stale result.
  TestCompletionCallback stale_creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        stale_creation_callback.callback()));
  }

  endpoint_request->set_is_stale_while_refreshing(true);
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(stale_creation_callback.WaitForResult(), IsOk());
  }

  // The job should succeed immediately, without waiting for fresh DNS!
  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv4Addr1));

  stale_data.ExpectAllReadDataConsumed();
  stale_data.ExpectAllWriteDataConsumed();
}

TEST_P(QuicSessionPoolAsyncDnsJobOptimisticDnsTest,
       StaleWinsDualRaceOverFresh) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // Stale connection attempt will succeed, but hang initially.
  MockQuicData stale_data(version_);
  stale_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  stale_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  stale_data.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();

  // Fresh connection attempt will hang in handshake.
  MockQuicData fresh_data(version_);
  fresh_data.AddConnect(SYNCHRONOUS, OK);
  fresh_data.AddReadPauseForever();
  // Fresh attempt is cancelled before it writes its settings packet.
  fresh_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  endpoint_request->set_is_stale_while_refreshing(true);
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    TestCompletionCallback stale_creation_callback;
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        stale_creation_callback.callback()));
    EXPECT_THAT(stale_creation_callback.WaitForResult(),
                IsError(ERR_IO_PENDING));
  }

  EXPECT_FALSE(callback_.have_result());

  // Fresh results arrive containing a different IP.
  endpoint_request->set_is_stale_while_refreshing(false);
  endpoint_request->set_endpoints({MakeUsableEndpoint(kIpv4Addr2)});
  endpoint_request->CallOnServiceEndpointsUpdated();

  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  if (async_quic_session()) {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // The stale attempt succeeds first, beating the fresh attempt.
  ASSERT_EQ(crypto_client_stream_factory_.streams().size(), 2u);
  crypto_client_stream_factory_.streams()[0]->NotifySessionOneRttKeyAvailable();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv4Addr1));

  stale_data.ExpectAllReadDataConsumed();
  stale_data.ExpectAllWriteDataConsumed();
  fresh_data.ExpectAllReadDataConsumed();
  fresh_data.ExpectAllWriteDataConsumed();
}

TEST_P(QuicSessionPoolAsyncDnsJobOptimisticDnsTest,
       FreshArrivesDuringStaleHandshake) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // Stale connection attempt will hang in handshake and be cancelled before
  // it writes its settings packet.
  MockQuicData stale_data(version_);
  stale_data.AddReadPauseForever();
  stale_data.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();

  // Fresh connection attempt will succeed.
  MockQuicData fresh_data(version_);
  fresh_data.AddConnect(SYNCHRONOUS, OK);
  fresh_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  fresh_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  fresh_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  // Resolver provides stale result.
  TestCompletionCallback stale_creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        stale_creation_callback.callback()));
  }

  endpoint_request->set_is_stale_while_refreshing(true);
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(stale_creation_callback.WaitForResult(),
                IsError(ERR_IO_PENDING));
  }

  // Stale attempt is active but pending crypto completion.
  EXPECT_FALSE(callback_.have_result());

  // Fresh results arrive containing a different IP.
  endpoint_request->set_is_stale_while_refreshing(false);
  endpoint_request->set_endpoints({MakeUsableEndpoint(kIpv4Addr2)});
  endpoint_request->CallOnServiceEndpointsUpdated();

  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  if (async_quic_session()) {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // The fresh attempt succeeds first.
  crypto_client_stream_factory_.streams()[1]->NotifySessionZeroRttComplete();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv4Addr2));

  stale_data.ExpectAllReadDataConsumed();
  stale_data.ExpectAllWriteDataConsumed();
  fresh_data.ExpectAllReadDataConsumed();
  fresh_data.ExpectAllWriteDataConsumed();
}

TEST_P(QuicSessionPoolAsyncDnsJobOptimisticDnsTest,
       FreshDnsReturnsSameAsStale) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // Stale connection attempt will hang in handshake.
  MockQuicData stale_data(version_);
  stale_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  stale_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  stale_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback stale_creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        stale_creation_callback.callback()));
  }

  endpoint_request->set_is_stale_while_refreshing(true);
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(stale_creation_callback.WaitForResult(),
                IsError(ERR_IO_PENDING));
  }

  // Fresh results arrive containing the SAME IP.
  endpoint_request->set_is_stale_while_refreshing(false);
  endpoint_request->set_endpoints({MakeUsableEndpoint(kIpv4Addr1)});
  endpoint_request->CallOnServiceEndpointsUpdated();

  // Notice NO new session creation is expected!
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  // Only the stale stream exists. Completing it completes the job.
  crypto_client_stream_factory_.streams()[0]->NotifySessionZeroRttComplete();
  crypto_client_stream_factory_.streams()[0]->NotifySessionOneRttKeyAvailable();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv4Addr1));

  stale_data.ExpectAllReadDataConsumed();
  stale_data.ExpectAllWriteDataConsumed();
}

TEST_P(QuicSessionPoolAsyncDnsJobOptimisticDnsTest,
       StaleSlowTimerDualFamilyHappyEyeballs) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // Stale IPv6 attempt hangs in handshake.
  MockQuicData ipv6_data(version_);
  ipv6_data.AddReadPauseForever();
  ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  // Stale IPv4 attempt started after slow timer succeeds.
  client_maker_.Reset();
  MockQuicData ipv4_data(version_);
  ipv4_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  ipv4_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  endpoint_request->set_is_stale_while_refreshing(true);
  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    TestCompletionCallback stale_creation_callback;
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        stale_creation_callback.callback()));
    EXPECT_THAT(stale_creation_callback.WaitForResult(),
                IsError(ERR_IO_PENDING));
  }

  // Primary connector starts IPv6 attempt.
  QuicChromiumClientSession* ipv6_session =
      GetPendingSession(kDefaultDestination);
  EXPECT_EQ(ToIPEndPoint(ipv6_session->peer_address()),
            MakeIPEndPoint(kIpv6Addr1));
  // Slow timer fires, spawning secondary connector on stale IPv4 in parallel.
  FastForwardBy(SlowTimerDelay());

  if (async_quic_session()) {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  ASSERT_EQ(crypto_client_stream_factory_.streams().size(), 2u);

  endpoint_request->set_is_stale_while_refreshing(false);
  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  crypto_client_stream_factory_.streams()[1]->NotifySessionZeroRttComplete();
  crypto_client_stream_factory_.streams()[1]->NotifySessionOneRttKeyAvailable();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv4Addr1));
  EXPECT_FALSE(QuicSessionPoolPeer::IsLiveSession(pool_.get(), ipv6_session));

  ipv4_data.ExpectAllReadDataConsumed();
  ipv4_data.ExpectAllWriteDataConsumed();
  ipv6_data.ExpectAllReadDataConsumed();
  ipv6_data.ExpectAllWriteDataConsumed();
}

TEST_P(QuicSessionPoolAsyncDnsJobOptimisticDnsTest, StaleFailsFreshFails) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();

  // Stale attempt fails.
  MockQuicData stale_data(version_);
  stale_data.AddConnect(SYNCHRONOUS, ERR_ADDRESS_UNREACHABLE);
  stale_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  endpoint_request->set_is_stale_while_refreshing(true);
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  EXPECT_FALSE(callback_.have_result());

  // Fresh DNS fails.
  endpoint_request->set_is_stale_while_refreshing(false);
  endpoint_request->set_endpoints({});
  endpoint_request->CallOnServiceEndpointRequestFinished(ERR_NAME_NOT_RESOLVED);

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_NAME_NOT_RESOLVED));

  stale_data.ExpectAllReadDataConsumed();
  stale_data.ExpectAllWriteDataConsumed();
}

TEST_P(QuicSessionPoolAsyncDnsJobOptimisticDnsTest,
       StaleFailsFreshSucceedsOnSameIP) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // Stale connection attempt will fail synchronously.
  MockQuicData stale_data(version_);
  stale_data.AddConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED);
  stale_data.AddSocketDataToFactory(socket_factory_.get());

  client_maker_.Reset();

  // Fresh connection attempt will succeed on the exact same IP.
  MockQuicData fresh_data(version_);
  fresh_data.AddConnect(SYNCHRONOUS, OK);
  fresh_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  fresh_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  fresh_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  endpoint_request->set_is_stale_while_refreshing(true);
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // The stale attempt has now failed. The job stays alive waiting for fresh
  // DNS. We complete fresh DNS, returning the exact same IP.
  endpoint_request->set_is_stale_while_refreshing(false);

  TestCompletionCallback fresh_creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        fresh_creation_callback.callback()));
    EXPECT_FALSE(fresh_creation_callback.have_result());
  }

  endpoint_request->CallOnServiceEndpointRequestFinished(OK);
  if (async_quic_session()) {
    EXPECT_THAT(fresh_creation_callback.WaitForResult(),
                IsError(ERR_IO_PENDING));
  }

  // The fresh attempt should now be active in handshake.
  ASSERT_EQ(crypto_client_stream_factory_.streams().size(), 1u);
  crypto_client_stream_factory_.streams()[0]->NotifySessionZeroRttComplete();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  stale_data.ExpectAllReadDataConsumed();
  stale_data.ExpectAllWriteDataConsumed();
  fresh_data.ExpectAllReadDataConsumed();
  fresh_data.ExpectAllWriteDataConsumed();
}

TEST_P(QuicSessionPoolAsyncDnsJobOptimisticDnsTest,
       FreshDnsFailsWhileStaleHangs) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // Stale connection attempt will hang in handshake.
  MockQuicData stale_data(version_);
  stale_data.AddConnect(SYNCHRONOUS, OK);
  stale_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  stale_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  TestCompletionCallback stale_creation_callback;
  if (async_quic_session()) {
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        stale_creation_callback.callback()));
  }

  endpoint_request->set_is_stale_while_refreshing(true);
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    EXPECT_THAT(stale_creation_callback.WaitForResult(),
                IsError(ERR_IO_PENDING));
  }

  // A hard DNS error on fresh DNS should immediately fail the job,
  // aborting the hanging stale attempt.
  endpoint_request->set_is_stale_while_refreshing(false);
  endpoint_request->CallOnServiceEndpointRequestFinished(ERR_NAME_NOT_RESOLVED);

  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_NAME_NOT_RESOLVED));

  stale_data.ExpectAllReadDataConsumed();
  stale_data.ExpectAllWriteDataConsumed();
}

TEST_P(QuicSessionPoolAsyncDnsJobOptimisticDnsTest,
       StaleFailsSessionCreationThenFreshPools) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request1 =
      fake_resolver_.AddFakeRequest();
  endpoint_request1->add_endpoint(MakeUsableEndpoint(kIpv4Addr2));
  endpoint_request1->CompleteStartAsynchronously(OK);

  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Setup an existing session on IP B.
  MockQuicData existing_data(version_);
  existing_data.AddConnect(SYNCHRONOUS, OK);
  existing_data.AddReadPauseForever();
  existing_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  existing_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder1(this);
  EXPECT_THAT(builder1.CallRequest(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  client_maker_.Reset();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);

  // Stale connection attempt will fail synchronously (IP A).
  MockQuicData stale_data(version_);
  stale_data.AddConnect(SYNCHRONOUS, ERR_CONNECTION_REFUSED);
  stale_data.AddSocketDataToFactory(socket_factory_.get());

  // Add the second request which will do the dual race.
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request2 =
      fake_resolver_.AddFakeRequest();

  TestCompletionCallback callback2;
  RequestBuilder builder2(this);
  builder2.url = GURL("https://mail.example.org/");
  builder2.destination = url::SchemeHostPort(
      url::kHttpsScheme, kServer2HostName, kDefaultServerPort);
  builder2.callback = callback2.callback();
  EXPECT_THAT(builder2.CallRequest(), IsError(ERR_IO_PENDING));

  endpoint_request2->set_is_stale_while_refreshing(true);
  endpoint_request2->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request2->set_crypto_ready(true);
  endpoint_request2->CallOnServiceEndpointsUpdated();

  // The stale attempt has now failed. The job stays alive waiting for fresh
  // DNS. We complete fresh DNS, returning IP B, which matches the existing
  // session.
  endpoint_request2->set_is_stale_while_refreshing(false);
  endpoint_request2->add_endpoint(MakeUsableEndpoint(kIpv4Addr2));
  endpoint_request2->CallOnServiceEndpointRequestFinished(OK);

  EXPECT_THAT(callback2.WaitForResult(), IsOk());

  stale_data.ExpectAllReadDataConsumed();
  stale_data.ExpectAllWriteDataConsumed();
  existing_data.ExpectAllReadDataConsumed();
  existing_data.ExpectAllWriteDataConsumed();
}

TEST_P(QuicSessionPoolAsyncDnsJobOptimisticDnsTest,
       EagerPoolingWhileAttemptsInFlight) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request1 =
      fake_resolver_.AddFakeRequest();
  endpoint_request1->add_endpoint(MakeUsableEndpoint(kIpv4Addr2));
  endpoint_request1->CompleteStartAsynchronously(OK);
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Setup an existing session on IP B.
  MockQuicData existing_data(version_);
  existing_data.AddConnect(SYNCHRONOUS, OK);
  existing_data.AddReadPauseForever();
  existing_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  existing_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder1(this);
  EXPECT_THAT(builder1.CallRequest(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  client_maker_.Reset();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // Second request will get IP A, which hangs.
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request2 =
      fake_resolver_.AddFakeRequest();

  MockQuicData hanging_data(version_);
  hanging_data.AddConnect(SYNCHRONOUS, OK);
  hanging_data.AddReadPauseForever();
  hanging_data.AddSocketDataToFactory(socket_factory_.get());

  TestCompletionCallback callback2;
  RequestBuilder builder2(this);
  builder2.url = GURL("https://mail.example.org/");
  builder2.destination =
      url::SchemeHostPort(url::kHttpsScheme, "mail.example.org", 443);
  builder2.callback = callback2.callback();
  EXPECT_THAT(builder2.CallRequest(), IsError(ERR_IO_PENDING));

  endpoint_request2->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request2->set_crypto_ready(true);
  endpoint_request2->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  EXPECT_FALSE(callback2.have_result());

  // Now, incrementally add IP B (which has the active session).
  // The job should eagerly pool to the existing session and complete,
  // without waiting for IP A to fail or finish.
  endpoint_request2->add_endpoint(MakeUsableEndpoint(kIpv4Addr2));
  endpoint_request2->CallOnServiceEndpointsUpdated();

  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  std::unique_ptr<QuicChromiumClientSession::Handle> handle1 =
      builder1.request.ReleaseSessionHandle();
  std::unique_ptr<QuicChromiumClientSession::Handle> handle2 =
      builder2.request.ReleaseSessionHandle();
  ASSERT_TRUE(handle1);
  ASSERT_TRUE(handle2);
  EXPECT_TRUE(handle2->SharesSameSession(*handle1));

  existing_data.ExpectAllReadDataConsumed();
  existing_data.ExpectAllWriteDataConsumed();
  hanging_data.ExpectAllReadDataConsumed();
  hanging_data.ExpectAllWriteDataConsumed();
}

TEST_P(QuicSessionPoolAsyncDnsJobOptimisticDnsTest,
       StaleEvaluatedForPoolingThenFreshPoolsOnSecondEndpoint) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request1 =
      fake_resolver_.AddFakeRequest();
  endpoint_request1->add_endpoint(MakeUsableEndpoint(kIpv4Addr2));
  endpoint_request1->CompleteStartAsynchronously(OK);

  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);

  // Setup an existing session on IP 2 (kIpv4Addr2).
  MockQuicData existing_data(version_);
  existing_data.AddConnect(SYNCHRONOUS, OK);
  existing_data.AddReadPauseForever();
  existing_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  existing_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder1(this);
  EXPECT_THAT(builder1.CallRequest(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(callback_.WaitForResult(), IsOk());

  client_maker_.Reset();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // Stale attempt will hang on IP 1 (kIpv4Addr1).
  MockQuicData stale_data(version_);
  stale_data.AddConnect(SYNCHRONOUS, OK);
  stale_data.AddReadPauseForever();
  stale_data.AddSocketDataToFactory(socket_factory_.get());

  // Second request for server2.
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request2 =
      fake_resolver_.AddFakeRequest();

  TestCompletionCallback callback2;
  RequestBuilder builder2(this);
  builder2.url = GURL("https://mail.example.org/");
  builder2.destination = url::SchemeHostPort(
      url::kHttpsScheme, kServer2HostName, kDefaultServerPort);
  builder2.callback = callback2.callback();
  EXPECT_THAT(builder2.CallRequest(), IsError(ERR_IO_PENDING));

  // Stale resolution delivers 2 endpoints: IP 1 and IP 3 (neither is IP 2).
  // This causes num_endpoints_evaluated_for_pooling_ to become 2.
  endpoint_request2->set_is_stale_while_refreshing(true);
  endpoint_request2->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request2->add_endpoint(MakeUsableEndpoint("192.168.0.3"));
  endpoint_request2->set_crypto_ready(true);
  endpoint_request2->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Fresh DNS arrives delivering 2 endpoints: IP 1 and IP 2 (IP 2 matches
  // existing session). Because num_endpoints_evaluated_for_pooling_ was reset
  // to 0, IP 2 at index 1 is evaluated and pooled to.
  endpoint_request2->set_is_stale_while_refreshing(false);
  endpoint_request2->set_endpoints(
      {MakeUsableEndpoint(kIpv4Addr1), MakeUsableEndpoint(kIpv4Addr2)});
  endpoint_request2->CallOnServiceEndpointRequestFinished(OK);

  EXPECT_THAT(callback2.WaitForResult(), IsOk());
  std::unique_ptr<QuicChromiumClientSession::Handle> handle1 =
      builder1.request.ReleaseSessionHandle();
  std::unique_ptr<QuicChromiumClientSession::Handle> handle2 =
      builder2.request.ReleaseSessionHandle();
  ASSERT_TRUE(handle1);
  ASSERT_TRUE(handle2);
  EXPECT_TRUE(handle2->SharesSameSession(*handle1));

  existing_data.ExpectAllReadDataConsumed();
  existing_data.ExpectAllWriteDataConsumed();
  stale_data.ExpectAllReadDataConsumed();
  stale_data.ExpectAllWriteDataConsumed();
}

TEST_P(QuicSessionPoolAsyncDnsJobOptimisticDnsTest,
       PromotedStaleAttemptFiresSlowTimerWithDeductedElapsed) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // Stale IPv4 attempt will hang in handshake and be cancelled before writing
  // its settings packet.
  MockQuicData ipv4_data(version_);
  ipv4_data.AddConnect(SYNCHRONOUS, OK);
  ipv4_data.AddReadPauseForever();
  ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  // Fresh IPv6 attempt will succeed.
  client_maker_.Reset();
  MockQuicData ipv6_data(version_);
  ipv6_data.AddConnect(SYNCHRONOUS, OK);
  ipv6_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_ZERO_RTT);
  ipv6_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  ipv6_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  endpoint_request->set_is_stale_while_refreshing(true);
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    TestCompletionCallback stale_creation_callback;
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        stale_creation_callback.callback()));
    EXPECT_THAT(stale_creation_callback.WaitForResult(),
                IsError(ERR_IO_PENDING));
  }

  // Half of the slow timer delay elapses while the stale IPv4 attempt runs.
  FastForwardBy(SlowTimerDelay() / 2);

  // Fresh results arrive with IPv6 and IPv4.
  // The stale IPv4 attempt is promoted to fresh_state_.primary_connector.
  // Its slow timer is armed with the remaining half of SlowTimerDelay().
  endpoint_request->set_is_stale_while_refreshing(false);
  endpoint_request->set_endpoints(
      {MakeUsableV6Endpoint(kIpv6Addr1), MakeUsableEndpoint(kIpv4Addr1)});
  endpoint_request->CallOnServiceEndpointsUpdated();

  // Advancing by only a quarter does not fire the slow timer yet.
  FastForwardBy(SlowTimerDelay() / 4);
  ASSERT_EQ(crypto_client_stream_factory_.streams().size(), 1u);

  // Advancing the remaining quarter fires the slow timer.
  FastForwardBy(SlowTimerDelay() / 4);

  if (async_quic_session()) {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // A second stream (IPv6 on secondary connector) should now be active.
  ASSERT_EQ(crypto_client_stream_factory_.streams().size(), 2u);

  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  // The fresh IPv6 attempt succeeds, winning the race.
  crypto_client_stream_factory_.streams()[1]->NotifySessionZeroRttComplete();
  crypto_client_stream_factory_.streams()[1]->NotifySessionOneRttKeyAvailable();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv6Addr1));

  ipv4_data.ExpectAllReadDataConsumed();
  ipv4_data.ExpectAllWriteDataConsumed();
  ipv6_data.ExpectAllReadDataConsumed();
  ipv6_data.ExpectAllWriteDataConsumed();
}

TEST_P(QuicSessionPoolAsyncDnsJobOptimisticDnsTest,
       PromoteStaleConnectors_IPv6SecondarySwappedToPrimary) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // Stale attempt (IPv6) will succeed.
  MockQuicData stale_ipv6_data(version_);
  stale_ipv6_data.AddConnect(SYNCHRONOUS, OK);
  stale_ipv6_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  stale_ipv6_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  stale_ipv6_data.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();

  // Fresh attempt (IPv4) will hang in handshake and get cancelled.
  MockQuicData fresh_ipv4_data(version_);
  fresh_ipv4_data.AddConnect(SYNCHRONOUS, OK);
  fresh_ipv4_data.AddReadPauseForever();
  fresh_ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  // 1. Stale DNS returns IPv6 first.
  endpoint_request->set_is_stale_while_refreshing(true);
  endpoint_request->add_endpoint(MakeUsableV6Endpoint(kIpv6Addr1));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    TestCompletionCallback stale_creation_callback;
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        stale_creation_callback.callback()));
    EXPECT_THAT(stale_creation_callback.WaitForResult(),
                IsError(ERR_IO_PENDING));
  }

  EXPECT_FALSE(callback_.have_result());

  // 2. Fresh DNS arrives initially with only IPv4.
  // The stale IPv6 connector is not in the fresh list, so it is not promoted.
  // Fresh state creates fresh_state_.primary_connector attempting IPv4.
  endpoint_request->set_is_stale_while_refreshing(false);
  endpoint_request->set_endpoints({MakeUsableEndpoint(kIpv4Addr1)});
  endpoint_request->CallOnServiceEndpointsUpdated();

  // 3. A subsequent fresh DNS update now includes the IPv6 endpoint.
  // Since fresh_state_.primary_connector already has an in-flight IPv4 attempt,
  // the stale IPv6 connector is promoted into fresh_state_.secondary_connector.
  // The slot swap detects an IPv4 primary and IPv6 secondary, and swaps them.
  endpoint_request->set_endpoints(
      {MakeUsableEndpoint(kIpv4Addr1), MakeUsableV6Endpoint(kIpv6Addr1)});
  endpoint_request->CallOnServiceEndpointsUpdated();

  EXPECT_FALSE(
      net_log_observer_
          .GetEntriesWithType(
              NetLogEventType::QUIC_SESSION_POOL_ASYNC_DNS_JOB_SLOTS_SWAPPED)
          .empty());

  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  if (async_quic_session()) {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Stream 0 is the stale IPv6 attempt. It succeeds and settles the job.
  ASSERT_GE(crypto_client_stream_factory_.streams().size(), 2u);
  crypto_client_stream_factory_.streams()[0]->NotifySessionOneRttKeyAvailable();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv6Addr1));

  auto settled_entries = net_log_observer_.GetEntriesWithType(
      NetLogEventType::QUIC_SESSION_POOL_ASYNC_DNS_JOB_CONNECTOR_SETTLED_JOB);
  ASSERT_EQ(settled_entries.size(), 1u);
  EXPECT_TRUE(settled_entries[0].params.FindList("canceled_attempts"));

  stale_ipv6_data.ExpectAllReadDataConsumed();
  stale_ipv6_data.ExpectAllWriteDataConsumed();
  fresh_ipv4_data.ExpectAllReadDataConsumed();
  fresh_ipv4_data.ExpectAllWriteDataConsumed();
}

TEST_P(QuicSessionPoolAsyncDnsJobOptimisticDnsTest,
       SlowTimerCancelledWhenSecondaryConnectorPromoted) {
  base::WeakPtr<FakeServiceEndpointRequest> endpoint_request =
      fake_resolver_.AddFakeRequest();
  InitializeWithFakeResolver();
  pool_->set_has_quic_ever_worked_on_current_network(true);
  ProofVerifyDetailsChromium verify_details = DefaultProofVerifyDetails();
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.AddProofVerifyDetails(&verify_details);
  crypto_client_stream_factory_.set_handshake_mode(
      MockCryptoClientStream::ASYNC_ZERO_RTT);

  // Stale attempt (IPv4 address 2) will succeed.
  MockQuicData stale_ipv4_data(version_);
  stale_ipv4_data.AddConnect(SYNCHRONOUS, OK);
  stale_ipv4_data.AddReadPauseForever();
  client_maker_.SetEncryptionLevel(quic::ENCRYPTION_FORWARD_SECURE);
  stale_ipv4_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  stale_ipv4_data.AddSocketDataToFactory(socket_factory_.get());
  client_maker_.Reset();

  // Fresh attempt (IPv4 address 1) will hang in handshake and get cancelled.
  MockQuicData fresh_ipv4_data(version_);
  fresh_ipv4_data.AddConnect(SYNCHRONOUS, OK);
  fresh_ipv4_data.AddReadPauseForever();
  fresh_ipv4_data.AddSocketDataToFactory(socket_factory_.get());

  RequestBuilder builder(this);
  EXPECT_THAT(builder.CallRequest(), IsError(ERR_IO_PENDING));

  // 1. Stale DNS returns IPv4 address 2 first.
  endpoint_request->set_is_stale_while_refreshing(true);
  endpoint_request->add_endpoint(MakeUsableEndpoint(kIpv4Addr2));
  endpoint_request->set_crypto_ready(true);
  endpoint_request->CallOnServiceEndpointsUpdated();

  if (async_quic_session()) {
    TestCompletionCallback stale_creation_callback;
    EXPECT_TRUE(builder.request.WaitForQuicSessionCreation(
        stale_creation_callback.callback()));
    EXPECT_THAT(stale_creation_callback.WaitForResult(),
                IsError(ERR_IO_PENDING));
  }

  EXPECT_FALSE(callback_.have_result());

  // 2. Fresh DNS arrives initially with only IPv4 address 1.
  // The stale connector (kIpv4Addr2) is not in the fresh list, so it is not
  // promoted. Fresh state creates fresh_state_.primary_connector attempting
  // kIpv4Addr1, which arms fresh_state_.slow_timer.
  endpoint_request->set_is_stale_while_refreshing(false);
  endpoint_request->set_endpoints({MakeUsableEndpoint(kIpv4Addr1)});
  endpoint_request->CallOnServiceEndpointsUpdated();

  // 3. A subsequent fresh DNS update adds kIpv4Addr2.
  // Since fresh_state_.primary_connector is busy with kIpv4Addr1, the stale
  // connector is promoted into fresh_state_.secondary_connector.
  // Promotion must stop fresh_state_.slow_timer.
  endpoint_request->set_endpoints(
      {MakeUsableEndpoint(kIpv4Addr1), MakeUsableEndpoint(kIpv4Addr2)});
  endpoint_request->CallOnServiceEndpointsUpdated();

  // 4. Fast-forward past the slow timer duration. If the slow timer was not
  // stopped, OnSlowTimer would fire and crash on
  // CHECK(!state->secondary_connector).
  FastForwardBy(SlowTimerDelay() * 2);

  endpoint_request->CallOnServiceEndpointRequestFinished(OK);

  if (async_quic_session()) {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Stale attempt (now in secondary connector) succeeds.
  ASSERT_GE(crypto_client_stream_factory_.streams().size(), 2u);
  crypto_client_stream_factory_.streams()[0]->NotifySessionOneRttKeyAvailable();

  EXPECT_THAT(callback_.WaitForResult(), IsOk());
  EXPECT_EQ(ToIPEndPoint(GetActiveSession(kDefaultDestination)->peer_address()),
            MakeIPEndPoint(kIpv4Addr2));

  stale_ipv4_data.ExpectAllReadDataConsumed();
  stale_ipv4_data.ExpectAllWriteDataConsumed();
  fresh_ipv4_data.ExpectAllReadDataConsumed();
  fresh_ipv4_data.ExpectAllWriteDataConsumed();
}

}  // namespace net::test
