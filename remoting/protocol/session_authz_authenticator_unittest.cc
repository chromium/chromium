// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/session_authz_authenticator.h"

#include <memory>
#include <optional>
#include <string_view>
#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "net/http/http_status_code.h"
#include "remoting/base/http_status.h"
#include "remoting/base/mock_session_authz_service_client.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/session_authz_service_client.h"
#include "remoting/base/session_policies.h"
#include "remoting/proto/session_authz_service.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/authenticator_test_base.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/credentials_type.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "remoting/protocol/spake2_authenticator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::protocol {
namespace {

using testing::_;
using testing::ByMove;
using testing::NiceMock;
using testing::Return;

constexpr std::string_view kFakeHostToken = "fake_host_token";
constexpr std::string_view kFakeSessionId = "fake_session_id";
constexpr std::string_view kFakeSessionToken = "fake_session_token";
constexpr std::string_view kFakeSharedSecret = "fake_shared_secret";
constexpr std::string_view kFakeSessionReauthToken =
    "fake_session_reauth_token";
constexpr base::TimeDelta kFakeSessionReauthTokenLifetime = base::Minutes(5);
constexpr int kMessageSize = 100;
constexpr int kMessages = 1;

auto RespondGenerateHostToken() {
  auto response = std::make_unique<internal::GenerateHostTokenResponseStruct>();
  response->host_token = kFakeHostToken;
  response->session_id = kFakeSessionId;
  return base::test::RunOnceCallback<0>(HttpStatus::OK(), std::move(response));
}

auto RespondVerifySessionToken(
    std::string_view session_id = kFakeSessionId,
    std::string_view shared_secret = kFakeSharedSecret,
    const std::optional<SessionPolicies>& session_policies = std::nullopt) {
  auto response =
      std::make_unique<internal::VerifySessionTokenResponseStruct>();
  response->session_id = session_id;
  response->shared_secret = shared_secret;
  response->session_policies = session_policies;
  response->session_reauth_token = kFakeSessionReauthToken;
  response->session_reauth_token_lifetime = kFakeSessionReauthTokenLifetime;
  return base::test::RunOnceCallback<1>(HttpStatus::OK(), std::move(response));
}

auto RespondVerifySessionTokenWithoutReauthFields() {
  auto response =
      std::make_unique<internal::VerifySessionTokenResponseStruct>();
  response->session_id = kFakeSessionId;
  response->shared_secret = kFakeSharedSecret;
  return base::test::RunOnceCallback<1>(HttpStatus::OK(), std::move(response));
}

class FakeClientAuthenticator : public Authenticator {
 public:
  FakeClientAuthenticator(
      const CreateBaseAuthenticatorCallback& create_base_authenticator_callback,
      CredentialsType credentials_type);
  ~FakeClientAuthenticator() override;

  // Authenticator implementation.
  CredentialsType credentials_type() const override;
  const Authenticator& implementing_authenticator() const override;
  State state() const override;
  bool started() const override;
  RejectionReason rejection_reason() const override;
  RejectionDetails rejection_details() const override;
  void ProcessMessage(const JingleAuthentication& message,
                      base::OnceClosure resume_callback) override;
  JingleAuthentication GetNextMessage() override;
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
  CredentialsType credentials_type_ = CredentialsType::CORP_SESSION_AUTHZ;
  std::unique_ptr<Authenticator> underlying_;
  std::string host_token_;
  JingleAuthentication message_;
  base::OnceClosure resume_callback_;
  bool underlying_authenticator_message_suppressed_ = false;
};

CredentialsType FakeClientAuthenticator::credentials_type() const {
  return credentials_type_;
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
    const CreateBaseAuthenticatorCallback& create_base_authenticator_callback,
    CredentialsType credentials_type)
    : create_base_authenticator_callback_(create_base_authenticator_callback),
      credentials_type_(credentials_type) {}

FakeClientAuthenticator::~FakeClientAuthenticator() = default;

Authenticator::RejectionReason FakeClientAuthenticator::rejection_reason()
    const {
  return session_authz_state_ == SessionAuthzState::FAILED
             ? session_authz_rejection_reason_
             : underlying_->rejection_reason();
}

Authenticator::RejectionDetails FakeClientAuthenticator::rejection_details()
    const {
  if (underlying_ && underlying_->state() == State::REJECTED) {
    return underlying_->rejection_details();
  }
  return {};
}

void FakeClientAuthenticator::ProcessMessage(
    const JingleAuthentication& message,
    base::OnceClosure resume_callback) {
  switch (session_authz_state_) {
    case SessionAuthzState::WAITING_FOR_HOST_TOKEN:
      host_token_ = message.session_authz_host_token;
      ASSERT_FALSE(host_token_.empty());
      session_authz_state_ = SessionAuthzState::READY_TO_SEND_SESSION_TOKEN;
      underlying_ = create_base_authenticator_callback_.Run(
          std::string(kFakeSharedSecret), MESSAGE_READY);
      std::move(resume_callback).Run();
      return;
    case SessionAuthzState::AUTHORIZED:
      underlying_->ProcessMessage(message, std::move(resume_callback));
      return;
    default:
      NOTREACHED();
  }
}

JingleAuthentication FakeClientAuthenticator::GetNextMessage() {
  EXPECT_EQ(state(), MESSAGE_READY);
  JingleAuthentication message;
  if (underlying_ && underlying_->state() == MESSAGE_READY) {
    if (underlying_authenticator_message_suppressed_) {
      underlying_->GetNextMessage();
    } else {
      message = underlying_->GetNextMessage();
    }
  }
  if (session_authz_state_ == SessionAuthzState::READY_TO_SEND_SESSION_TOKEN) {
    message.session_authz_session_token = std::string(kFakeSessionToken);
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

  // Used to create a paired set of authenticators using the provided
  // |credentials_type|.
  void ConfigureAuthenticators(CredentialsType credentials_type);

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
  ConfigureAuthenticators(CredentialsType::CORP_SESSION_AUTHZ);
}

void SessionAuthzAuthenticatorTest::ConfigureAuthenticators(
    CredentialsType credentials_type) {
  auto mock_service_client = std::make_unique<MockSessionAuthzServiceClient>();
  mock_service_client_ = mock_service_client.get();
  auto host_authenticator = std::make_unique<SessionAuthzAuthenticator>(
      credentials_type, std::move(mock_service_client),
      base::BindRepeating(&Spake2Authenticator::CreateForHost, kHostId,
                          kClientId, host_cert_, key_pair_));
  host_authenticator_ = host_authenticator.get();
  host_ = std::move(host_authenticator);
  auto client_authenticator = std::make_unique<FakeClientAuthenticator>(
      base::BindRepeating(&Spake2Authenticator::CreateForClient, kClientId,
                          kHostId),
      credentials_type);
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
  EXPECT_CALL(*mock_service_client_, VerifySessionToken(kFakeSessionToken, _))
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
  SessionPolicies policies;
  policies.maximum_session_duration = base::Hours(10);
  policies.curtain_required = true;
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
      .Run(HttpStatus(HttpStatus::Code::PERMISSION_DENIED, "Permission denied"),
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
      .Run(HttpStatus(HttpStatus::Code::PERMISSION_DENIED, "Permission denied"),
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
            Authenticator::RejectionReason::INVALID_ARGUMENT);
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

TEST_F(SessionAuthzAuthenticatorTest,
       SessionReauthToken_NotPresent_AllowedForCloudHost) {
  ConfigureAuthenticators(CredentialsType::CLOUD_SESSION_AUTHZ);
  EXPECT_CALL(*mock_service_client_, GenerateHostToken(_))
      .WillOnce(RespondGenerateHostToken());
  EXPECT_CALL(*mock_service_client_, VerifySessionToken(_, _))
      .WillOnce(RespondVerifySessionTokenWithoutReauthFields());

  StartAuthExchange();

  ASSERT_EQ(host_->state(), Authenticator::ACCEPTED);
}

TEST_F(SessionAuthzAuthenticatorTest,
       SessionReauthToken_NotPresent_NotAllowedForCorpHost) {
  ConfigureAuthenticators(CredentialsType::CORP_SESSION_AUTHZ);
  auto response =
      std::make_unique<internal::VerifySessionTokenResponseStruct>();
  response->session_id = kFakeSessionId;
  response->shared_secret = kFakeSharedSecret;
  EXPECT_CALL(*mock_service_client_, GenerateHostToken(_))
      .WillOnce(RespondGenerateHostToken());
  EXPECT_CALL(*mock_service_client_, VerifySessionToken(_, _))
      .WillOnce(RespondVerifySessionTokenWithoutReauthFields());

  StartAuthExchange();

  ASSERT_EQ(host_->state(), Authenticator::REJECTED);
  ASSERT_EQ(host_->rejection_reason(),
            Authenticator::RejectionReason::UNEXPECTED_ERROR);
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

  EXPECT_CALL(*mock_service_client_,
              ReauthorizeHost(kFakeSessionReauthToken, kFakeSessionId, _, _))
      .WillOnce(base::test::RunOnceCallback<3>(
          HttpStatus(net::HttpStatusCode::HTTP_FORBIDDEN), nullptr));
  EXPECT_CALL(state_change_after_accepted_callback, Run());

  task_environment_.FastForwardBy(kFakeSessionReauthTokenLifetime);

  ASSERT_EQ(host_->state(), Authenticator::REJECTED);
  ASSERT_EQ(host_->rejection_reason(),
            Authenticator::RejectionReason::REAUTHZ_POLICY_CHECK_FAILED);
}

class SessionAuthzAuthenticatorTeardownTest : public AuthenticatorTestBase {
 protected:
  void SetUp() override {
    AuthenticatorTestBase::SetUp();
    // Do not call ConfigureAuthenticators() here. We manually configure the
    // host authenticator with a mock in each test case to simulate synchronous
    // teardown. This simplifies the test by avoiding the need to pull in the
    // additional dependencies required for a full session teardown cascade
    // (e.g., JingleSession, ClientSession, and ChromotingHost).
  }
};

TEST_F(SessionAuthzAuthenticatorTeardownTest,
       UnderlyingAuthenticator_InvalidIncomingMessage_SynchronousTeardown) {
  // Use a mock underlying authenticator.
  auto mock_underlying_owned = std::make_unique<NiceMock<MockAuthenticator>>();
  MockAuthenticator* mock_underlying = mock_underlying_owned.get();

  auto mock_service_client = std::make_unique<MockSessionAuthzServiceClient>();
  MockSessionAuthzServiceClient* mock_service_client_ptr =
      mock_service_client.get();

  EXPECT_CALL(*mock_service_client_ptr, GenerateHostToken(_))
      .WillOnce(RespondGenerateHostToken());
  EXPECT_CALL(*mock_service_client_ptr, VerifySessionToken(_, _))
      .WillOnce(RespondVerifySessionToken());

  auto mock_underlying_holder = base::MakeRefCounted<
      base::RefCountedData<std::unique_ptr<Authenticator>>>(
      std::move(mock_underlying_owned));
  auto host_authenticator = std::make_unique<SessionAuthzAuthenticator>(
      CredentialsType::CORP_SESSION_AUTHZ, std::move(mock_service_client),
      base::BindRepeating(
          [](scoped_refptr<base::RefCountedData<std::unique_ptr<Authenticator>>>
                 holder,
             const std::string& secret,
             Authenticator::State state) { return std::move(holder->data); },
          mock_underlying_holder));
  SessionAuthzAuthenticator* host_authenticator_ptr = host_authenticator.get();
  host_ = std::move(host_authenticator);

  base::test::TestFuture<void> start_future;
  host_authenticator_ptr->Start(start_future.GetCallback());
  EXPECT_TRUE(start_future.Wait());

  // Transition to WAITING_FOR_SESSION_TOKEN state.
  std::ignore = host_->GetNextMessage();

  // First ProcessMessage triggers VerifySessionToken, which then triggers
  // underlying_->ProcessMessage.
  JingleAuthentication message;
  message.session_authz_session_token = std::string(kFakeSessionToken);

  // We want underlying_->ProcessMessage to synchronously run the callback
  // and we want to destroy host_ inside that callback.
  EXPECT_CALL(*mock_underlying, ProcessMessage(_, _))
      .WillOnce([&](const JingleAuthentication&, base::OnceClosure callback) {
        std::move(callback).Run();
      });

  // Also need to mock state() because StartReauthorizerIfNecessary calls it.
  ON_CALL(*mock_underlying, state())
      .WillByDefault(Return(Authenticator::REJECTED));

  host_->ProcessMessage(
      message,
      base::BindOnce(
          [](std::unique_ptr<Authenticator>* host) { host->reset(); }, &host_));

  ASSERT_EQ(host_, nullptr);
}

TEST_F(SessionAuthzAuthenticatorTeardownTest,
       UnderlyingAuthenticator_SubsequentMessage_SynchronousTeardown) {
  // Use a mock underlying authenticator.
  auto mock_underlying_owned = std::make_unique<NiceMock<MockAuthenticator>>();
  MockAuthenticator* mock_underlying = mock_underlying_owned.get();

  auto mock_service_client = std::make_unique<MockSessionAuthzServiceClient>();
  MockSessionAuthzServiceClient* mock_service_client_ptr =
      mock_service_client.get();

  EXPECT_CALL(*mock_service_client_ptr, GenerateHostToken(_))
      .WillOnce(RespondGenerateHostToken());
  EXPECT_CALL(*mock_service_client_ptr, VerifySessionToken(_, _))
      .WillOnce(RespondVerifySessionToken());

  // Use a ref-counted holder to allow the repeating callback to return the
  // unique_ptr once.
  auto mock_underlying_holder = base::MakeRefCounted<
      base::RefCountedData<std::unique_ptr<Authenticator>>>(
      std::move(mock_underlying_owned));
  host_ = std::make_unique<SessionAuthzAuthenticator>(
      CredentialsType::CORP_SESSION_AUTHZ, std::move(mock_service_client),
      base::BindRepeating(
          [](scoped_refptr<base::RefCountedData<std::unique_ptr<Authenticator>>>
                 holder,
             const std::string& secret,
             Authenticator::State state) { return std::move(holder->data); },
          mock_underlying_holder));

  base::test::TestFuture<void> start_future;
  static_cast<SessionAuthzAuthenticator*>(host_.get())
      ->Start(start_future.GetCallback());
  EXPECT_TRUE(start_future.Wait());

  std::ignore = host_->GetNextMessage();
  JingleAuthentication message;
  message.session_authz_session_token = std::string(kFakeSessionToken);

  // First ProcessMessage triggers VerifySessionToken, which then triggers
  // underlying_->ProcessMessage.
  // This time we DON'T destroy it yet.
  EXPECT_CALL(*mock_underlying, ProcessMessage(_, _))
      .WillOnce([&](const JingleAuthentication&, base::OnceClosure callback) {
        std::move(callback).Run();
      });
  EXPECT_CALL(*mock_underlying, state())
      .WillRepeatedly(Return(Authenticator::WAITING_MESSAGE));

  host_->ProcessMessage(message, base::DoNothing());

  // Now the state should be SHARED_SECRET_FETCHED.
  // Call ProcessMessage again with a callback that destroys host_.
  EXPECT_CALL(*mock_underlying, ProcessMessage(_, _))
      .WillOnce([&](const JingleAuthentication&, base::OnceClosure callback) {
        std::move(callback).Run();
      });

  host_->ProcessMessage(message,
                        base::BindOnce(
                            [](std::unique_ptr<Authenticator>* host,
                               MockSessionAuthzServiceClient** client_ptr) {
                              *client_ptr = nullptr;
                              host->reset();
                            },
                            &host_, &mock_service_client_ptr));

  ASSERT_EQ(host_, nullptr);
}

}  // namespace remoting::protocol
