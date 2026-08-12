// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
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
#include "net/quic/address_utils.h"
#include "net/quic/mock_crypto_client_stream.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_http_stream.h"
#include "net/quic/quic_session_pool.h"
#include "net/quic/quic_session_pool_test_base.h"
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
  // All features go through the base fixture, which settles them before any
  // test activity. A second ScopedFeatureList would swap the feature state
  // again while the task environment threads are already running.
  static std::vector<base::test::FeatureRef> EnabledFeatures() {
    std::vector<base::test::FeatureRef> enabled = {features::kAsyncDnsQuicJob};
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

  QuicSessionPoolAsyncDnsJobTest()
      : QuicSessionPoolTestBase(DefaultSupportedQuicVersions().front(),
                                EnabledFeatures(),
                                DisabledFeatures()) {}

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
        &crypto_client_stream_factory_, &context_);
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

  IPEndPoint MakeIPEndPoint(std::string_view addr) {
    IPAddress ip_address;
    CHECK(ip_address.AssignFromIPLiteral(addr));
    return IPEndPoint(ip_address, kDefaultServerPort);
  }

  FakeServiceEndpointResolver fake_resolver_;
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
  endpoint_request2->add_endpoint(MakeUsableEndpoint("192.168.0.1"));
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

}  // namespace

}  // namespace net::test
