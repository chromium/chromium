// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_attempt_manager.h"

#include <memory>
#include <optional>
#include <set>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_chain.h"
#include "net/base/reconnect_notifier.h"
#include "net/base/session_usage.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/log/net_log_source_type.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/mock_quic_data.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_endpoint.h"
#include "net/quic/quic_session_alias_key.h"
#include "net/quic/quic_session_attempt_request.h"
#include "net/quic/quic_session_pool_test_base.h"
#include "net/socket/socket_tag.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/multiplexed_session_creation_initiator.h"
#include "net/test/gtest_util.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace net::test {

namespace {

IPEndPoint MakeIPEndPoint(std::string_view addr, uint16_t port = 443) {
  return IPEndPoint(*IPAddress::FromIPLiteral(addr), port);
}

class SessionRequester {
 public:
  explicit SessionRequester(QuicSessionAttemptManager* manager,
                            quic::ParsedQuicVersion version)
      : manager_(manager),
        quic_version_(version),
        net_log_(NetLogWithSource::Make(NetLog::Get(),
                                        NetLogSourceType::URL_REQUEST)) {}

  SessionRequester(SessionRequester&&) = default;
  SessionRequester& operator=(SessionRequester&&) = default;
  SessionRequester(const SessionRequester&) = delete;
  SessionRequester& operator=(const SessionRequester&) = delete;

  ~SessionRequester() = default;

  SessionRequester& SetDestination(url::SchemeHostPort destination) {
    destination_ = std::move(destination);
    return *this;
  }

  SessionRequester& SetIPEndPoint(IPEndPoint ip_endpoint) {
    endpoint_ = QuicEndpoint(quic_version_, std::move(ip_endpoint),
                             ConnectionEndpointMetadata());
    return *this;
  }

  int Request() {
    QuicSessionAliasKey key(
        destination_,
        QuicSessionKey(destination_.host(), destination_.port(), privacy_mode_,
                       proxy_chain_, session_usage_, socket_tag_,
                       network_anonymization_key_, secure_dns_policy_,
                       require_dns_https_alpn_,
                       disable_cert_verification_network_fetches_));
    request_ = manager_->CreateRequest(key);
    int rv = request_->RequestSession(
        endpoint_, cert_verify_flags_, dns_resolution_start_time_,
        dns_resolution_end_time_, /*use_dns_aliases=*/true, dns_aliases_,
        session_creation_initiator_, connection_management_config_, net_log_,
        base::BindOnce(&SessionRequester::OnComplete, base::Unretained(this)));
    if (rv != ERR_IO_PENDING) {
      OnComplete(rv);
    }
    return rv;
  }

  int WaitForResult() {
    if (result_.has_value()) {
      return *result_;
    }

    CHECK(!wait_for_result_closure_);
    base::RunLoop run_loop;
    wait_for_result_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    CHECK(result_.has_value());
    return *result_;
  }

  void ResetRequest() { request_.reset(); }

  const QuicSessionAliasKey& key() const {
    CHECK(request_);
    return request_->key();
  }

  std::optional<int> result() const { return result_; }

  QuicChromiumClientSession* session() { return session_; }
  const QuicChromiumClientSession* session() const { return session_; }
  const NetErrorDetails& error_details() const { return error_details_; }

  const url::SchemeHostPort& destination() const { return destination_; }

 private:
  void OnComplete(int result) {
    CHECK(!result_.has_value());
    CHECK(request_);

    result_ = result;
    if (result_ == OK) {
      session_ = request_->session();
    } else {
      error_details_ = request_->error_details();
    }
    request_.reset();
    // Clear the request to avoid dangling pointer.
    manager_ = nullptr;

    if (wait_for_result_closure_) {
      std::move(wait_for_result_closure_).Run();
    }
  }

  raw_ptr<QuicSessionAttemptManager> manager_;
  quic::ParsedQuicVersion quic_version_;

  // For calculating the session key.
  url::SchemeHostPort destination_{
      url::kHttpsScheme, QuicSessionPoolTestBase::kDefaultServerHostName,
      QuicSessionPoolTestBase::kDefaultServerPort};
  PrivacyMode privacy_mode_ = PRIVACY_MODE_DISABLED;
  ProxyChain proxy_chain_ = ProxyChain::Direct();
  SessionUsage session_usage_ = SessionUsage::kDestination;
  SocketTag socket_tag_;
  NetworkAnonymizationKey network_anonymization_key_;
  SecureDnsPolicy secure_dns_policy_ = SecureDnsPolicy::kAllow;
  bool require_dns_https_alpn_ = false;
  bool disable_cert_verification_network_fetches_ = false;

  // For calling RequestSession().
  QuicEndpoint endpoint_{quic_version_,
                         IPEndPoint(IPAddress::IPv4Localhost(), 443),
                         ConnectionEndpointMetadata()};
  int cert_verify_flags_ = 0;
  base::TimeTicks dns_resolution_start_time_ = base::TimeTicks::Now();
  base::TimeTicks dns_resolution_end_time_ = base::TimeTicks::Now();
  std::set<std::string> dns_aliases_;
  MultiplexedSessionCreationInitiator session_creation_initiator_ =
      MultiplexedSessionCreationInitiator::kUnknown;
  std::optional<ConnectionManagementConfig> connection_management_config_;
  NetLogWithSource net_log_;

  std::unique_ptr<QuicSessionAttemptRequest> request_;

  base::OnceClosure wait_for_result_closure_;

  std::optional<int> result_;
  raw_ptr<QuicChromiumClientSession> session_ = nullptr;
  NetErrorDetails error_details_;
};

}  // namespace

class QuicSessionAttemptManagerTest
    : public QuicSessionPoolTestBase,
      public ::testing::TestWithParam<quic::ParsedQuicVersion> {
 protected:
  QuicSessionAttemptManagerTest() : QuicSessionPoolTestBase(GetParam()) {}

  void InitializeWithDefaultProofVerifyDetails() {
    Initialize();
    crypto_client_stream_factory_.AddProofVerifyDetails(
        &default_verify_details_);
  }

  SessionRequester CreateRequester() {
    return SessionRequester(pool_->session_attempt_manager(), GetParam());
  }

  QuicSessionAttemptManager* session_attempt_manager() {
    return pool_->session_attempt_manager();
  }

 private:
  ProofVerifyDetailsChromium default_verify_details_ =
      DefaultProofVerifyDetails();
};

INSTANTIATE_TEST_SUITE_P(/**/,
                         QuicSessionAttemptManagerTest,
                         ::testing::ValuesIn(AllSupportedQuicVersions()));

TEST_P(QuicSessionAttemptManagerTest, RequestSessionSync) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(net::features::kAsyncQuicSession);

  InitializeWithDefaultProofVerifyDetails();

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddWrite(SYNCHRONOUS, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  SessionRequester requester = CreateRequester();
  int result = requester.Request();
  EXPECT_THAT(result, IsOk());
  EXPECT_TRUE(requester.session());
}

TEST_P(QuicSessionAttemptManagerTest, RequestSessionSyncFailure) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(net::features::kAsyncQuicSession);

  InitializeWithDefaultProofVerifyDetails();

  MockQuicData socket_data(version_);
  socket_data.AddConnect(ASYNC, ERR_CONNECTION_REFUSED);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  SessionRequester requester = CreateRequester();
  int result = requester.Request();
  EXPECT_THAT(result, IsError(ERR_CONNECTION_REFUSED));
}

TEST_P(QuicSessionAttemptManagerTest, RequestSessionAsync) {
  InitializeWithDefaultProofVerifyDetails();

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddWrite(ASYNC, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  SessionRequester requester = CreateRequester();
  int result = requester.Request();
  EXPECT_THAT(result, IsError(ERR_IO_PENDING));

  result = requester.WaitForResult();
  EXPECT_THAT(result, IsOk());
}

TEST_P(QuicSessionAttemptManagerTest, MultipleRequestsSameSessionSuccess) {
  InitializeWithDefaultProofVerifyDetails();

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddWrite(ASYNC, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create three requesters for the same session.
  SessionRequester requester1 = CreateRequester();
  SessionRequester requester2 = CreateRequester();
  SessionRequester requester3 = CreateRequester();

  EXPECT_THAT(requester1.Request(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(requester2.Request(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(requester3.Request(), IsError(ERR_IO_PENDING));

  // All requesters should succeed and get the same session.
  EXPECT_THAT(requester1.WaitForResult(), IsOk());
  EXPECT_THAT(requester2.WaitForResult(), IsOk());
  EXPECT_THAT(requester3.WaitForResult(), IsOk());

  EXPECT_TRUE(requester1.session());
  EXPECT_EQ(requester1.session(), requester2.session());
  EXPECT_EQ(requester1.session(), requester3.session());
}

TEST_P(QuicSessionAttemptManagerTest, MultipleRequestsSameSessionFailure) {
  InitializeWithDefaultProofVerifyDetails();

  MockQuicData socket_data(version_);
  socket_data.AddConnect(ASYNC, ERR_ADDRESS_IN_USE);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create three requesters for the same session.
  SessionRequester requester1 = CreateRequester();
  SessionRequester requester2 = CreateRequester();
  SessionRequester requester3 = CreateRequester();

  EXPECT_THAT(requester1.Request(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(requester2.Request(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(requester3.Request(), IsError(ERR_IO_PENDING));

  // All requesters should fail with the same error.
  EXPECT_THAT(requester1.WaitForResult(), IsError(ERR_ADDRESS_IN_USE));
  EXPECT_THAT(requester2.WaitForResult(), IsError(ERR_ADDRESS_IN_USE));
  EXPECT_THAT(requester3.WaitForResult(), IsError(ERR_ADDRESS_IN_USE));

  EXPECT_FALSE(requester1.session());
  EXPECT_FALSE(requester2.session());
  EXPECT_FALSE(requester3.session());
}

// Test multiple endpoints for the same session: One succeeds, one fails.
TEST_P(QuicSessionAttemptManagerTest, MultipleEndpointsSuccessAndFailure) {
  InitializeWithDefaultProofVerifyDetails();

  // Create two mock sockets for two different endpoints.
  MockQuicData socket_data1(version_);
  socket_data1.AddConnect(ASYNC, ERR_CONNECTION_FAILED);
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData socket_data2(version_);
  socket_data2.AddReadPauseForever();
  socket_data2.AddWrite(ASYNC, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  SessionRequester requester1 = CreateRequester();
  requester1.SetIPEndPoint(MakeIPEndPoint("192.0.2.1"));
  SessionRequester requester2 = CreateRequester();
  requester2.SetIPEndPoint(MakeIPEndPoint("192.0.2.2"));

  EXPECT_THAT(requester1.Request(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(requester2.Request(), IsError(ERR_IO_PENDING));

  // First endpoint fails but second succeeds, so both requesters should
  // receive the successful session.
  EXPECT_THAT(requester1.WaitForResult(), IsOk());
  EXPECT_THAT(requester2.WaitForResult(), IsOk());

  EXPECT_TRUE(requester1.session());
  EXPECT_TRUE(requester2.session());
  EXPECT_EQ(requester1.session(), requester2.session());
}

// Test multiple endpoints for the same session: All fail.
TEST_P(QuicSessionAttemptManagerTest, MultipleEndpointsAllFail) {
  InitializeWithDefaultProofVerifyDetails();

  // Create three mock sockets for three different endpoints, all fail
  MockQuicData socket_data1(version_);
  socket_data1.AddConnect(ASYNC, ERR_CONNECTION_FAILED);
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData socket_data2(version_);
  socket_data2.AddConnect(ASYNC, ERR_ADDRESS_UNREACHABLE);
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  MockQuicData socket_data3(version_);
  socket_data3.AddConnect(ASYNC, ERR_CONNECTION_REFUSED);
  socket_data3.AddSocketDataToFactory(socket_factory_.get());

  SessionRequester requester1 = CreateRequester();
  requester1.SetIPEndPoint(MakeIPEndPoint("2001:db8::1"));
  SessionRequester requester2 = CreateRequester();
  requester2.SetIPEndPoint(MakeIPEndPoint("2001:db8::2"));
  SessionRequester requester3 = CreateRequester();
  requester3.SetIPEndPoint(MakeIPEndPoint("2001:db8::3"));

  EXPECT_THAT(requester1.Request(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(requester2.Request(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(requester3.Request(), IsError(ERR_IO_PENDING));

  // All should fail with the last error.
  EXPECT_THAT(requester1.WaitForResult(), IsError(ERR_CONNECTION_REFUSED));
  EXPECT_THAT(requester2.WaitForResult(), IsError(ERR_CONNECTION_REFUSED));
  EXPECT_THAT(requester3.WaitForResult(), IsError(ERR_CONNECTION_REFUSED));

  // No sessions should be created.
  EXPECT_FALSE(requester1.session());
  EXPECT_FALSE(requester2.session());
  EXPECT_FALSE(requester3.session());
}

// Test multiple endpoints for the same session: All sessions succeed,
// but only the first one is used.
TEST_P(QuicSessionAttemptManagerTest, MultipleEndpointsAllSuccess) {
  InitializeWithDefaultProofVerifyDetails();

  MockQuicData socket_data1(version_);
  socket_data1.AddReadPauseForever();
  socket_data1.AddWrite(ASYNC, ConstructInitialSettingsPacket());
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  // This would succeed but should be cancelled.
  MockQuicData socket_data2(version_);
  socket_data2.AddReadPauseForever();
  socket_data2.AddWrite(ASYNC, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  SessionRequester requester1 = CreateRequester();
  requester1.SetIPEndPoint(MakeIPEndPoint("2001:db8::1"));
  SessionRequester requester2 = CreateRequester();
  requester2.SetIPEndPoint(MakeIPEndPoint("192.0.2.1"));

  EXPECT_THAT(requester1.Request(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(requester2.Request(), IsError(ERR_IO_PENDING));

  // Both should succeed and have the same session.
  EXPECT_THAT(requester1.WaitForResult(), IsOk());
  EXPECT_THAT(requester2.WaitForResult(), IsOk());

  EXPECT_TRUE(requester1.session());
  EXPECT_TRUE(requester2.session());
  EXPECT_EQ(requester1.session(), requester2.session());
}

TEST_P(QuicSessionAttemptManagerTest, JobCompletesWhenAllRequestsCancelled) {
  InitializeWithDefaultProofVerifyDetails();

  MockQuicData socket_data(version_);
  socket_data.AddConnect(ASYNC, ERR_IO_PENDING);
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create multiple pending requests.
  SessionRequester requester1 = CreateRequester();
  SessionRequester requester2 = CreateRequester();
  SessionRequester requester3 = CreateRequester();
  EXPECT_THAT(requester1.Request(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(requester2.Request(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(requester3.Request(), IsError(ERR_IO_PENDING));

  const QuicSessionAliasKey key = requester1.key();

  // Cancel first two requests.
  requester1.ResetRequest();
  requester2.ResetRequest();

  // At this point, the Job should still exist because requester3 is active.
  EXPECT_TRUE(session_attempt_manager()->HasActiveJobForTesting(key));

  // Cancel the last request. This should destroy the Job.
  requester3.ResetRequest();
  EXPECT_FALSE(session_attempt_manager()->HasActiveJobForTesting(key));
}

TEST_P(QuicSessionAttemptManagerTest, CancelSomeRequestsWhileOthersComplete) {
  InitializeWithDefaultProofVerifyDetails();

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddWrite(ASYNC, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create multiple pending requests.
  SessionRequester requester1 = CreateRequester();
  SessionRequester requester2 = CreateRequester();
  SessionRequester requester3 = CreateRequester();
  SessionRequester requester4 = CreateRequester();
  EXPECT_THAT(requester1.Request(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(requester2.Request(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(requester3.Request(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(requester4.Request(), IsError(ERR_IO_PENDING));

  // Cancel two requests before connection completes.
  requester1.ResetRequest();
  requester2.ResetRequest();

  // Complete connection attempt. Remaining requests should succeed.
  EXPECT_THAT(requester3.WaitForResult(), IsOk());
  EXPECT_THAT(requester4.WaitForResult(), IsOk());

  EXPECT_TRUE(requester3.session());
  EXPECT_TRUE(requester4.session());
  EXPECT_EQ(requester3.session(), requester4.session());
}

TEST_P(QuicSessionAttemptManagerTest, DestroyManagerWithPendingRequests) {
  InitializeWithDefaultProofVerifyDetails();

  MockQuicData socket_data(version_);
  socket_data.AddReadPauseForever();
  socket_data.AddWrite(ASYNC, ConstructInitialSettingsPacket());
  socket_data.AddSocketDataToFactory(socket_factory_.get());

  // Create multiple pending requests.
  SessionRequester requester1 = CreateRequester();
  SessionRequester requester2 = CreateRequester();
  SessionRequester requester3 = CreateRequester();

  EXPECT_THAT(requester1.Request(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(requester2.Request(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(requester3.Request(), IsError(ERR_IO_PENDING));

  // Destroy the pool (which contains the manager) while requests are
  // pending.
  pool_.reset();

  EXPECT_THAT(requester1.WaitForResult(), IsError(ERR_ABORTED));
  EXPECT_THAT(requester2.WaitForResult(), IsError(ERR_ABORTED));
  EXPECT_THAT(requester3.WaitForResult(), IsError(ERR_ABORTED));
}

TEST_P(QuicSessionAttemptManagerTest, OnOriginFrame) {
  InitializeWithDefaultProofVerifyDetails();

  // 1. Make a request for `kDefaultServerHostName` that will be pending.
  MockQuicData socket_data1(version_);
  socket_data1.AddConnect(ASYNC, ERR_IO_PENDING);
  socket_data1.AddSocketDataToFactory(socket_factory_.get());

  SessionRequester requester1 = CreateRequester();
  requester1.SetIPEndPoint(MakeIPEndPoint("192.0.2.1"));
  EXPECT_THAT(requester1.Request(), IsError(ERR_IO_PENDING));
  EXPECT_TRUE(
      session_attempt_manager()->HasActiveJobForTesting(requester1.key()));
  const QuicSessionAliasKey key1 = requester1.key();

  // 2. Create a new session for `other.example.org`.
  const char* kOtherHostname = "other.example.org";
  const url::SchemeHostPort kOtherDestination(url::kHttpsScheme, kOtherHostname,
                                              443);

  MockQuicData socket_data2(version_);
  socket_data2.AddReadPauseForever();
  socket_data2.AddWrite(ASYNC, ConstructInitialSettingsPacket());
  socket_data2.AddSocketDataToFactory(socket_factory_.get());

  SessionRequester requester2 = CreateRequester();
  requester2.SetDestination(kOtherDestination);
  requester2.SetIPEndPoint(MakeIPEndPoint("192.0.2.2"));
  EXPECT_THAT(requester2.Request(), IsError(ERR_IO_PENDING));
  EXPECT_THAT(requester2.WaitForResult(), IsOk());
  QuicChromiumClientSession* session2 = requester2.session();
  EXPECT_TRUE(session2);

  // 3. Simulate an Origin Frame reception.
  quic::OriginFrame origin_frame;
  origin_frame.origins.emplace_back(requester1.destination().Serialize());
  session2->OnOriginFrame(origin_frame);

  // 4. The first request should complete with `session2`.
  EXPECT_THAT(requester1.WaitForResult(), IsOk());
  EXPECT_EQ(requester1.session(), session2);

  // The job for `requester1` should be gone.
  EXPECT_FALSE(session_attempt_manager()->HasActiveJobForTesting(key1));
}

}  // namespace net::test
