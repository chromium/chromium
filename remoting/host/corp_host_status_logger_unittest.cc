// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/corp_host_status_logger.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "remoting/base/local_session_policies_provider.h"
#include "remoting/base/logging_service_client.h"
#include "remoting/base/session_policies.h"
#include "remoting/proto/logging_service.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/credentials_type.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/session_authz_authenticator.h"
#include "remoting/protocol/session_authz_reauthorizer.h"
#include "remoting/protocol/session_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace {

using testing::_;
using testing::Return;
using testing::ReturnRef;

constexpr char kFakeSessionId[] = "fake_session_id";
constexpr char kFakeHostToken[] = "fake_host_token";
constexpr char kFakeReauthToken[] = "fake_reauth_token";

const SessionPolicies FakeLocalSessionPolicies() {
  SessionPolicies session_policies;
  session_policies.allow_relayed_connections = true;
  session_policies.allow_file_transfer = false;

  return session_policies;
}

class MockLoggingServiceClient final : public LoggingServiceClient {
 public:
  MockLoggingServiceClient() = default;
  ~MockLoggingServiceClient() override = default;

  MockLoggingServiceClient(const MockLoggingServiceClient&) = delete;
  MockLoggingServiceClient& operator=(const MockLoggingServiceClient&) = delete;

  MOCK_METHOD(void,
              ReportSessionDisconnected,
              (const internal::ReportSessionDisconnectedRequestStruct&,
               StatusCallback));
};

}  // namespace

class CorpHostStatusLoggerTest : public testing::Test {
 public:
  CorpHostStatusLoggerTest();
  ~CorpHostStatusLoggerTest() override;

 protected:
  void SetUpSessionAuthzAuthenticator();

  LocalSessionPoliciesProvider local_session_policies_provider_;
  raw_ptr<MockLoggingServiceClient> service_client_;
  std::unique_ptr<CorpHostStatusLogger> logger_;
  raw_ptr<protocol::SessionObserver> logger_as_observer_;
  base::MockOnceClosure unsubscribe_closure_;
  protocol::MockSession session_;
  protocol::MockSessionManager session_manager_;
  protocol::MockAuthenticator authenticator_;
  protocol::SessionAuthzAuthenticator session_authz_authenticator_{
      protocol::CredentialsType::CORP_SESSION_AUTHZ, nullptr,
      base::NullCallback()};
};

CorpHostStatusLoggerTest::CorpHostStatusLoggerTest() {
  auto service_client = std::make_unique<MockLoggingServiceClient>();
  service_client_ = service_client.get();
  local_session_policies_provider_.set_local_policies(
      FakeLocalSessionPolicies());
  logger_ = std::make_unique<CorpHostStatusLogger>(
      std::move(service_client), &local_session_policies_provider_);
  logger_as_observer_ = logger_.get();

  EXPECT_CALL(session_, authenticator())
      .WillRepeatedly(ReturnRef(authenticator_));
  EXPECT_CALL(session_manager_, AddSessionObserver(logger_.get()))
      .WillOnce(Return(
          protocol::SessionObserver::Subscription(unsubscribe_closure_.Get())));
  logger_->StartObserving(session_manager_);
}

CorpHostStatusLoggerTest::~CorpHostStatusLoggerTest() {
  EXPECT_CALL(unsubscribe_closure_, Run());
  service_client_ = nullptr;
  logger_as_observer_ = nullptr;
  logger_.reset();
}

void CorpHostStatusLoggerTest::SetUpSessionAuthzAuthenticator() {
  EXPECT_CALL(authenticator_, credentials_type())
      .WillOnce(Return(session_authz_authenticator_.credentials_type()));
  EXPECT_CALL(authenticator_, implementing_authenticator())
      .WillOnce(
          ReturnRef(session_authz_authenticator_.implementing_authenticator()));
  EXPECT_CALL(authenticator_, state())
      .WillRepeatedly(Return(protocol::Authenticator::State::ACCEPTED));
  EXPECT_CALL(authenticator_, GetSessionPolicies())
      .WillRepeatedly(Return(nullptr));
  session_authz_authenticator_.SetSessionIdForTesting(kFakeSessionId);
  session_authz_authenticator_.SetHostTokenForTesting(kFakeHostToken);
  session_authz_authenticator_.SetReauthorizerForTesting(
      std::make_unique<protocol::SessionAuthzReauthorizer>(
          nullptr, kFakeSessionId, kFakeReauthToken, base::Minutes(5),
          base::DoNothing()));
}

TEST_F(CorpHostStatusLoggerTest, UnsubscribeOnceDestroyed) {
  // Test done in the destructor.
}

TEST_F(CorpHostStatusLoggerTest, IgnoreUninterestingState) {
  EXPECT_CALL(*service_client_, ReportSessionDisconnected(_, _)).Times(0);

  logger_as_observer_->OnSessionStateChange(
      session_, protocol::Session::State::AUTHENTICATING);
}

TEST_F(CorpHostStatusLoggerTest, IgnoreNonSessionAuthzSession) {
  EXPECT_CALL(*service_client_, ReportSessionDisconnected(_, _)).Times(0);
  EXPECT_CALL(authenticator_, credentials_type())
      .WillOnce(Return(protocol::CredentialsType::SHARED_SECRET));

  logger_as_observer_->OnSessionStateChange(session_,
                                            protocol::Session::State::CLOSED);
}

TEST_F(CorpHostStatusLoggerTest, IgnoreSessionWithoutSessionAuthzId) {
  EXPECT_CALL(*service_client_, ReportSessionDisconnected(_, _)).Times(0);
  SetUpSessionAuthzAuthenticator();
  session_authz_authenticator_.SetSessionIdForTesting("");

  logger_as_observer_->OnSessionStateChange(session_,
                                            protocol::Session::State::CLOSED);
}

TEST_F(CorpHostStatusLoggerTest,
       ReportsSessionDisconnectedForSessionWithoutReauthorizer) {
  EXPECT_CALL(session_, error()).WillOnce(Return(ErrorCode::PEER_IS_OFFLINE));
  internal::ReportSessionDisconnectedRequestStruct expected_request;
  expected_request.session_authz_id = kFakeSessionId;
  expected_request.host_token = kFakeHostToken;
  expected_request.error_code = ErrorCode::PEER_IS_OFFLINE;
  expected_request.effective_session_policies = FakeLocalSessionPolicies();
  EXPECT_CALL(*service_client_, ReportSessionDisconnected(expected_request, _));
  SetUpSessionAuthzAuthenticator();
  session_authz_authenticator_.SetReauthorizerForTesting(nullptr);

  logger_as_observer_->OnSessionStateChange(session_,
                                            protocol::Session::State::FAILED);
}

TEST_F(CorpHostStatusLoggerTest,
       ReportsSessionDisconnectedForSessionWithoutHostTokenAndReauthorizer) {
  EXPECT_CALL(session_, error()).WillOnce(Return(ErrorCode::PEER_IS_OFFLINE));
  internal::ReportSessionDisconnectedRequestStruct expected_request;
  expected_request.session_authz_id = kFakeSessionId;
  expected_request.error_code = ErrorCode::PEER_IS_OFFLINE;
  expected_request.effective_session_policies = FakeLocalSessionPolicies();
  EXPECT_CALL(*service_client_, ReportSessionDisconnected(expected_request, _));
  SetUpSessionAuthzAuthenticator();
  session_authz_authenticator_.SetHostTokenForTesting({});
  session_authz_authenticator_.SetReauthorizerForTesting(nullptr);

  logger_as_observer_->OnSessionStateChange(session_,
                                            protocol::Session::State::FAILED);
}

TEST_F(CorpHostStatusLoggerTest, ReportsSessionDisconnectedForClosed) {
  EXPECT_CALL(session_, error()).WillOnce(Return(ErrorCode::OK));
  internal::ReportSessionDisconnectedRequestStruct expected_request;
  expected_request.session_authz_id = kFakeSessionId;
  expected_request.host_token = kFakeHostToken;
  expected_request.session_authz_reauth_token = kFakeReauthToken;
  expected_request.error_code = ErrorCode::OK;
  expected_request.effective_session_policies = FakeLocalSessionPolicies();
  EXPECT_CALL(*service_client_, ReportSessionDisconnected(expected_request, _));
  SetUpSessionAuthzAuthenticator();

  logger_as_observer_->OnSessionStateChange(session_,
                                            protocol::Session::State::CLOSED);
}

TEST_F(CorpHostStatusLoggerTest, ReportsSessionDisconnectedForFailed) {
  EXPECT_CALL(session_, error()).WillOnce(Return(ErrorCode::PEER_IS_OFFLINE));
  internal::ReportSessionDisconnectedRequestStruct expected_request;
  expected_request.session_authz_id = kFakeSessionId;
  expected_request.host_token = kFakeHostToken;
  expected_request.session_authz_reauth_token = kFakeReauthToken;
  expected_request.error_code = ErrorCode::PEER_IS_OFFLINE;
  expected_request.effective_session_policies = FakeLocalSessionPolicies();
  EXPECT_CALL(*service_client_, ReportSessionDisconnected(expected_request, _));
  SetUpSessionAuthzAuthenticator();

  logger_as_observer_->OnSessionStateChange(session_,
                                            protocol::Session::State::FAILED);
}

TEST_F(CorpHostStatusLoggerTest,
       AuthenticatorHasSessionPolicies_ReportsItInsteadOfLocalPolicies) {
  SessionPolicies authenticator_session_policies;
  authenticator_session_policies.allow_relayed_connections = false;
  authenticator_session_policies.allow_file_transfer = true;
  EXPECT_NE(authenticator_session_policies, FakeLocalSessionPolicies());
  EXPECT_CALL(session_, error()).WillOnce(Return(ErrorCode::OK));
  internal::ReportSessionDisconnectedRequestStruct expected_request;
  expected_request.session_authz_id = kFakeSessionId;
  expected_request.host_token = kFakeHostToken;
  expected_request.session_authz_reauth_token = kFakeReauthToken;
  expected_request.error_code = ErrorCode::OK;
  expected_request.effective_session_policies = authenticator_session_policies;
  EXPECT_CALL(*service_client_, ReportSessionDisconnected(expected_request, _));
  SetUpSessionAuthzAuthenticator();
  EXPECT_CALL(authenticator_, GetSessionPolicies())
      .WillRepeatedly(Return(&authenticator_session_policies));

  logger_as_observer_->OnSessionStateChange(session_,
                                            protocol::Session::State::CLOSED);
}

TEST_F(CorpHostStatusLoggerTest,
       AuthenticatorIsNotAccepted_DoesNotReportSessionPolicies) {
  EXPECT_CALL(session_, error()).WillOnce(Return(ErrorCode::PEER_IS_OFFLINE));
  internal::ReportSessionDisconnectedRequestStruct expected_request;
  expected_request.session_authz_id = kFakeSessionId;
  expected_request.host_token = kFakeHostToken;
  expected_request.session_authz_reauth_token = kFakeReauthToken;
  expected_request.error_code = ErrorCode::PEER_IS_OFFLINE;
  EXPECT_CALL(*service_client_, ReportSessionDisconnected(expected_request, _));
  SetUpSessionAuthzAuthenticator();
  EXPECT_CALL(authenticator_, state())
      .WillRepeatedly(
          Return(protocol::Authenticator::State::PROCESSING_MESSAGE));
  EXPECT_CALL(authenticator_, GetSessionPolicies()).Times(0);

  logger_as_observer_->OnSessionStateChange(session_,
                                            protocol::Session::State::FAILED);
}

}  // namespace remoting
