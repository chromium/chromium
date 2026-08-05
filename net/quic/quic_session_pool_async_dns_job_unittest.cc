// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/http/http_stream.h"
#include "net/quic/mock_crypto_client_stream.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_session_pool.h"
#include "net/quic/quic_session_pool_test_base.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace

}  // namespace net::test
