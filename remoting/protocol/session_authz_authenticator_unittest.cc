// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/session_authz_authenticator.h"

#include <memory>
#include <optional>
#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "net/http/http_status_code.h"
#include "remoting/base/mock_session_authz_service_client.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/session_authz_service_client.h"
#include "remoting/base/session_policies.h"
#include "remoting/proto/session_authz_service.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/authenticator_test_base.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/credentials_type.h"
#include "remoting/protocol/spake2_authenticator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::protocol {
namespace {

using testing::_;
using testing::ByMove;
using testing::Return;

constexpr char kFakeHostToken[] = "fake_host_token";
constexpr char kFakeSessionId[] = "fake_session_id";
constexpr char kFakeSessionToken[] = "fake_session_token";
constexpr char kFakeSharedSecret[] = "fake_shared_secret";
constexpr char kFakeSessionReauthToken[] = "fake_session_reauth_token";
constexpr base::TimeDelta kFakeSessionReauthTokenLifetime = base::Minutes(5);
constexpr int kMessageSize = 100;
constexpr int kMessages = 1;

auto RespondGenerateHostToken() {
  auto response = std::make_unique<internal::GenerateHostTokenResponseStruct>();
  response->host_token = kFakeHostToken;
  response->session_id = kFakeSessionId;
  return base::test::RunOnceCallback<0>(ProtobufHttpStatus::OK(),
                                        std::move(response));
}

auto RespondVerifySessionToken(
    const std::string& session_id = kFakeSessionId,
    const std::string& shared_secret = kFakeSharedSecret,
    const std::optional<SessionPolicies>& session_policies = std::nullopt) {
  auto response =
      std::make_unique<internal::VerifySessionTokenResponseStruct>();
  response->session_id = session_id;
  response->shared_secret = shared_secret;
  response->session_policies = session_policies;
  response->session_reauth_token = kFakeSessionReauthToken;
  response->session_reauth_token_lifetime = kFakeSessionReauthTokenLifetime;
  return base::test::RunOnceCallback<1>(ProtobufHttpStatus::OK(),
                                        std::move(response));
}

class FakeClientAuthenticator : public Authenticator {
 public:
  explicit FakeClientAuthenticator(const CreateBaseAuthenticatorCallback&
                                       create_base_authenticator_callback);
  ~FakeClientAuthenticator() override;

  // Authenticator implementation.
  CredentialsType credentials_type() const override;
  const Authenticator& implementing_authenticator() const override;
  State state() const override;
  bool started() const override;
  RejectionReason rejection_reason() const override;
  void ProcessMessage(const jingle_xmpp::XmlElement* message,
                      base::OnceClosure resume_callback) override;
  std::unique_ptr<jingle_xmpp::XmlElement> GetNextMessage() override;
  const std::string& GetAuthKey() const override;
  const SessionPolicies* GetSessionPolicies() const override;
  std::unique_ptr<ChannelAuthenticator> CreateChannelAuthenticator()
      const override;

  const std::string& host_token() const { return host_token_; }

  // Used to simulate an invalid message from the underlying authenticator.
  void set_underlying_authenticator_message_suppressed(bool suppressed) {
    underlying_authenticator_message_suppressed_ = suppressed;
  }

 private:
  enum class SessionAuthzState {
    WAITING_FOR_HOST_TOKEN,
    READY_TO_SEND_SESSION_TOKEN,
    AUTHORIZED,
    FAILED,
  };

  SessionAuthzState session_authz_state_ =
      SessionAuthzState::WAITING_FOR_HOST_TOKEN;
  RejectionReason session_authz_rejection_reason_;
  CreateBaseAuthenticatorCallback create_base_authenticator_callback_;
  std::unique_ptr<Authenticator> underlying_;
  std::string host_token_;
  std::unique_ptr<jingle_xmpp::XmlElement> message_;
  base::OnceClosure resume_callback_;
  bool underlying_authenticator_message_suppressed_ = false;
};

CredentialsType FakeClientAuthenticator::credentials_type() const {
  return CredentialsType::CORP_SESSION_AUTHZ;
}

const Authenticator& FakeClientAuthenticator::implementing_authenticator()
    const {
  return *this;
}

Authenticator::State FakeClientAuthenticator::state() const {
  switch (session_authz_state_) {
    case SessionAuthzState::READY_TO_SEND_SESSION_TOKEN:
      return MESSAGE_READY;
    case SessionAuthzState::WAITING_FOR_HOST_TOKEN:
      return WAITING_MESSAGE;
    case SessionAuthzState::AUTHORIZED:
      return underlying_->state();
    case SessionAuthzState::FAILED:
      return REJECTED;
  }
}

bool FakeClientAuthenticator::started() const {
  return session_authz_state_ != SessionAuthzState::WAITING_FOR_HOST_TOKEN;
}

FakeClientAuthenticator::FakeClientAuthenticator(
    const CreateBaseAuthenticatorCallback& create_base_authenticator_callback)
    : create_base_authenticator_callback_(create_base_authenticator_callback) {}

FakeClientAuthenticator::~FakeClientAuthenticator() = default;

Authenticator::RejectionReason FakeClientAuthenticator::rejection_reason()
    const {
  return session_authz_state_ == SessionAuthzState::FAILED
             ? session_authz_rejection_reason_
             : underlying_->rejection_reason();
}

void FakeClientAuthenticator::ProcessMessage(
    const jingle_xmpp::XmlElement* message,
    base::OnceClosure resume_callback) {
  switch (session_authz_state_) {
    case SessionAuthzState::WAITING_FOR_HOST_TOKEN:
      host_token_ =
          message->TextNamed(SessionAuthzAuthenticator::kHostTokenTag);
      ASSERT_FALSE(host_token_.empty());
      session_authz_state_ = SessionAuthzState::READY_TO_SEND_SESSION_TOKEN;
      underlying_ = create_base_authenticator_callback_.Run(kFakeSharedSecret,
                                                            MESSAGE_READY);
      std::move(resume_callback).Run();
      return;
    case SessionAuthzState::AUTHORIZED:
      underlying_->ProcessMessage(message, std::move(resume_callback));
      return;
    default:
      NOTREACHED();
  }
}

std::unique_ptr<jingle_xmpp::XmlElement>
FakeClientAuthenticator::GetNextMessage() {
  EXPECT_EQ(state(), MESSAGE_READY);
  std::unique_ptr<jingle_xmpp::XmlElement> message;
  if (underlying_ && underlying_->state() == MESSAGE_READY) {
    if (underlying_authenticator_message_suppressed_) {
      underlying_->GetNextMessage();
    } else {
      message = underlying_->GetNextMessage();
    }
  }
  if (!message) {
    message = CreateEmptyAuthenticatorMessage();
  }
  if (session_authz_state_ == SessionAuthzState::READY_TO_SEND_SESSION_TOKEN) {
    jingle_xmpp::XmlElement* session_token_element =
        new jingle_xmpp::XmlElement(
            SessionAuthzAuthenticator::kSessionTokenTag);
    session_token_element->SetBodyText(kFakeSessionToken);
    message->AddElement(session_token_element);
    session_authz_state_ = SessionAuthzState::AUTHORIZED;
  }
  return message;
}

const std::string& FakeClientAuthenticator::GetAuthKey() const {
  EXPECT_EQ(state(), ACCEPTED);
  return underlying_->GetAuthKey();
}

const SessionPolicies* FakeClientAuthenticator::GetSessionPolicies() const {
  EXPECT_EQ(state(), ACCEPTED);
  return nullptr;
}

std::unique_ptr<ChannelAuthenticator>
FakeClientAuthenticator::CreateChannelAuthenticator() const {
  EXPECT_EQ(state(), ACCEPTED);
  return underlying_->CreateChannelAuthenticator();
}

}  // namespace

class SessionAuthzAuthenticatorTest : public AuthenticatorTestBase {
 public:
  SessionAuthzAuthenticatorTest();
  ~SessionAuthzAuthenticatorTest() override;

 protected:
  void SetUp() override;

  // Caller must add an expectation for
  // mock_service_client_->GenerateHostToken() and run the callback, otherwise
  // this method will not return.
  void StartAuthExchange();

  raw_ptr<MockSessionAuthzServiceClient> mock_service_client_;
  raw_ptr<SessionAuthzAuthenticator> host_authenticator_;
  raw_ptr<FakeClientAuthenticator> client_authenticator_;
};

SessionAuthzAuthenticatorTest::SessionAuthzAuthenticatorTest() = default;
SessionAuthzAuthenticatorTest::~SessionAuthzAuthenticatorTest() = default;

void SessionAuthzAuthenticatorTest::SetUp() {
  AuthenticatorTestBase::SetUp();
  auto mock_service_client = std::make_unique<MockSessionAuthzServiceClient>();
  mock_service_client_ = mock_service_client.get();
  auto host_authenticator = std::make_unique<SessionAuthzAuthenticator>(
      CredentialsType::CORP_SESSION_AUTHZ, std::move(mock_service_client),
      base::BindRepeating(&Spake2Authenticator::CreateForHost, kHostId,
                          kClientId, host_cert_, key_pair_));
  host_authenticator_ = host_authenticator.get();
  host_ = std::move(host_authenticator);
  auto client_authenticator =
      std::make_unique<FakeClientAuthenticator>(base::BindRepeating(
          &Spake2Authenticator::CreateForClient, kClientId, kHostId));
  client_authenticator_ = client_authenticator.get();
  client_ = std::move(client_authenticator);
}

void SessionAuthzAuthenticatorTest::StartAuthExchange() {
  base::RunLoop run_loop;
  host_authenticator_->Start(run_loop.QuitClosure());
  run_loop.Run();
  RunHostInitiatedAuthExchange();
}

TEST_F(SessionAuthzAuthenticatorTest, SuccessfulAuth) {
  EXPECT_CALL(*mock_service_client_, GenerateHostToken(_))
      .WillOnce(RespondGenerateHostToken());
  internal::VerifySessionTokenRequestStruct
      expected_verify_session_token_request;
  expected_verify_session_token_request.session_token = kFakeSessionToken;
  EXPECT_CALL(*mock_service_client_,
              VerifySessionToken(expected_verify_session_token_request, _))
      .WillOnce(RespondVerifySessionToken());

  StartAuthExchange();
  ASSERT_EQ(host_->state(), Authenticator::ACCEPTED);
  ASSERT_EQ(client_->state(), Authenticator::ACCEPTED);
  ASSERT_EQ(client_authenticator_->host_token(), kFakeHostToken);

  // Verify that authenticated channels can be created after authentication.
  client_auth_ = client_->CreateChannelAuthenticator();
  host_auth_ = host_->CreateChannelAuthenticator();
  RunChannelAuth(false);

  StreamConnectionTester tester(host_socket_.get(), client_socket_.get(),
                                kMessageSize, kMessages);

  base::RunLoop run_loop;
  tester.Start(run_loop.QuitClosure());
  run_loop.Run();
  tester.CheckResults();
}

TEST_F(SessionAuthzAuthenticatorTest,
       AuthenticatedWithoutSessionPolicies_GetSessionPoliciesReturnsNullptr) {
  EXPECT_CALL(*mock_service_client_, GenerateHostToken(_))
      .WillOnce(RespondGenerateHostToken());
  EXPECT_CALL(*mock_service_client_, VerifySessionToken(_, _))
      .WillOnce(RespondVerifySessionToken());

  StartAuthExchange();
  ASSERT_EQ(host_->state(), Authenticator::ACCEPTED);
  ASSERT_EQ(host_->GetSessionPolicies(), nullptr);
}

TEST_F(SessionAuthzAuthenticatorTest,
       AuthenticatedWithSessionPolicies_GetSessionPoliciesReturnsPolicies) {
  SessionPolicies policies = {
      .maximum_session_duration = base::Hours(10),
      .curtain_required = true,
  };
  EXPECT_CALL(*mock_service_client_, GenerateHostToken(_))
      .WillOnce(RespondGenerateHostToken());
  EXPECT_CALL(*mock_service_client_, VerifySessionToken(_, _))
      .WillOnce(RespondVerifySessionToken(kFakeSessionId, kFakeSharedSecret,
                                          policies));

  StartAuthExchange();
  ASSERT_EQ(host_->state(), Authenticator::ACCEPTED);
  ASSERT_EQ(*host_->GetSessionPolicies(), policies);
}

TEST_F(SessionAuthzAuthenticatorTest, GenerateHostToken_RpcError_Rejected) {
  base::MockCallback<base::OnceClosure> mock_resume_callback;
  SessionAuthzServiceClient::GenerateHostTokenCallback
      generate_host_token_callback;
  EXPECT_CALL(*mock_service_client_, GenerateHostToken(_))
      .WillOnce(MoveArg<0>(&generate_host_token_callback));
  EXPECT_CALL(mock_resume_callback, Run()).Times(0);

  host_authenticator_->Start(mock_resume_callback.Get());
  ASSERT_EQ(host_->state(), Authenticator::PROCESSING_MESSAGE);

  EXPECT_CALL(mock_resume_callback, Run()).Times(1);

  std::move(generate_host_token_callback)
      .Run(ProtobufHttpStatus(ProtobufHttpStatus::Code::PERMISSION_DENIED,
                              "Permission denied"),
           nullptr);
  ASSERT_EQ(host_->state(), Authenticator::REJECTED);
  ASSERT_EQ(host_->rejection_reason(),
            Authenticator::RejectionReason::AUTHZ_POLICY_CHECK_FAILED);
}

TEST_F(SessionAuthzAuthenticatorTest, VerifySessionToken_RpcError_Rejected) {
  SessionAuthzServiceClient::VerifySessionTokenCallback
      verify_session_token_callback;
  EXPECT_CALL(*mock_service_client_, GenerateHostToken(_))
      .WillOnce(RespondGenerateHostToken());
  EXPECT_CALL(*mock_service_client_, VerifySessionToken(_, _))
      .WillOnce(MoveArg<1>(&verify_session_token_callback));

  StartAuthExchange();
  ASSERT_EQ(host_->state(), Authenticator::PROCESSING_MESSAGE);
  std::move(verify_session_token_callback)
      .Run(ProtobufHttpStatus(ProtobufHttpStatus::Code::PERMISSION_DENIED,
                              "Permission denied"),
           nullptr);
  ASSERT_EQ(host_->state(), Authenticator::REJECTED);
  ASSERT_EQ(host_->rejection_reason(),
            Authenticator::RejectionReason::AUTHZ_POLICY_CHECK_FAILED);
}

TEST_F(SessionAuthzAuthenticatorTest,
       VerifySessionToken_SessionIdMismatch_Rejected) {
  EXPECT_CALL(*mock_service_client_, GenerateHostToken(_))
      .WillOnce(RespondGenerateHostToken());
  EXPECT_CALL(*mock_service_client_, VerifySessionToken(_, _))
      .WillOnce(RespondVerifySessionToken("mismatched_session_id"));

  StartAuthExchange();
  ASSERT_EQ(host_->state(), Authenticator::REJECTED);
  ASSERT_EQ(host_->rejection_reason(),
            Authenticator::RejectionReason::INVALID_ACCOUNT_ID);
}

TEST_F(SessionAuthzAuthenticatorTest,
       UnderlyingAuthenticator_InvalidIncomingMessage_Rejected) {
  client_authenticator_->set_underlying_authenticator_message_suppressed(true);
  EXPECT_CALL(*mock_service_client_, GenerateHostToken(_))
      .WillOnce(RespondGenerateHostToken());
  EXPECT_CALL(*mock_service_client_, VerifySessionToken(_, _))
      .WillOnce(RespondVerifySessionToken());

  StartAuthExchange();
  ASSERT_EQ(host_->state(), Authenticator::REJECTED);
  ASSERT_EQ(host_->rejection_reason(),
            Authenticator::RejectionReason::PROTOCOL_ERROR);
}

TEST_F(SessionAuthzAuthenticatorTest,
       UnderlyingAuthenticator_MismatchedSharedSecret_NotAccepted) {
  EXPECT_CALL(*mock_service_client_, GenerateHostToken(_))
      .WillOnce(RespondGenerateHostToken());
  EXPECT_CALL(*mock_service_client_, VerifySessionToken(_, _))
      .WillOnce(RespondVerifySessionToken(kFakeSessionId,
                                          "mismatched_shared_secret"));

  StartAuthExchange();

  // With the mismatched shared secret, the underlying authenticator will just
  // believe the client has not sent enough data to complete the key exchange.
  ASSERT_NE(host_->state(), Authenticator::ACCEPTED);
}

TEST_F(SessionAuthzAuthenticatorTest, ReauthorizationFailed_Rejected) {
  EXPECT_CALL(*mock_service_client_, GenerateHostToken(_))
      .WillOnce(RespondGenerateHostToken());
  EXPECT_CALL(*mock_service_client_, VerifySessionToken(_, _))
      .WillOnce(RespondVerifySessionToken());
  base::MockCallback<base::RepeatingClosure>
      state_change_after_accepted_callback;
  host_->set_state_change_after_accepted_callback(
      state_change_after_accepted_callback.Get());
  EXPECT_CALL(state_change_after_accepted_callback, Run()).Times(0);

  StartAuthExchange();
  ASSERT_EQ(host_->state(), Authenticator::ACCEPTED);
  ASSERT_EQ(client_->state(), Authenticator::ACCEPTED);

  internal::ReauthorizeHostRequestStruct request;
  request.session_id = kFakeSessionId;
  request.session_reauth_token = kFakeSessionReauthToken;
  EXPECT_CALL(*mock_service_client_, ReauthorizeHost(request, _))
      .WillOnce(base::test::RunOnceCallback<1>(
          ProtobufHttpStatus(net::HttpStatusCode::HTTP_FORBIDDEN), nullptr));
  EXPECT_CALL(state_change_after_accepted_callback, Run());

  task_environment_.FastForwardBy(kFakeSessionReauthTokenLifetime);

  ASSERT_EQ(host_->state(), Authenticator::REJECTED);
  ASSERT_EQ(host_->rejection_reason(),
            Authenticator::RejectionReason::REAUTHZ_POLICY_CHECK_FAILED);
}

}  // namespace remoting::protocol
