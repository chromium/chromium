// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/client_session.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/errors.h"
#include "remoting/base/local_session_policies_provider.h"
#include "remoting/base/session_options_constants.h"
#include "remoting/base/session_policies.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/fake_desktop_environment.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/peer_session.h"
#include "remoting/protocol/capability_names.h"
#include "remoting/protocol/fake_session.h"
#include "remoting/protocol/ice_config_fetcher.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

using protocol::FakeSession;
using protocol::MockClientStub;

using testing::_;
using testing::AtMost;
using testing::ByMove;
using testing::Return;

class ClientSessionTest : public testing::Test {
 public:
  ClientSessionTest() = default;

  void SetUp() override;
  void TearDown() override;

 protected:
  // Creates the client session from a FakeSession instance.
  void CreateClientSession(std::unique_ptr<protocol::FakeSession> session);

  // Creates the client session.
  void CreateClientSession();

  // Notifies the client session that the client connection has been
  // authenticated and channels have been connected.
  void AuthenticateClientSession(
      const SessionPolicies* session_policies = nullptr);
  void ConnectClientSession(const SessionPolicies* session_policies = nullptr);

  // Message loop that will process all ClientSession tasks.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // AutoThreadTaskRunner on which `client_session_` will be run.
  scoped_refptr<AutoThreadTaskRunner> task_runner_;

  // Used to run `task_environment_` after each test, until no objects remain
  // that require it.
  base::RunLoop run_loop_;

  // HostExtensions to pass when creating the ClientSession. Caller retains
  // ownership of the HostExtensions themselves.
  std::vector<raw_ptr<HostExtension, VectorExperimental>> extensions_;

  SessionPolicies initial_local_policies_;
  LocalSessionPoliciesProvider local_session_policies_provider_;

  // ClientSession instance under test.
  std::unique_ptr<ClientSession> client_session_;

  // ClientSession::EventHandler mock for use in tests.
  MockClientSessionEventHandler session_event_handler_;

  MockPeerSessionFactory mock_peer_session_factory_;

  // Stubs returned to `client_session_` components by mock peer session.
  MockClientStub client_stub_;
  raw_ptr<MockPeerSession> mock_peer_session_ = nullptr;

  raw_ptr<protocol::FakeSession, DisableDanglingPtrDetection> session_;
  DesktopEnvironmentOptions desktop_environment_options_;
};

void ClientSessionTest::SetUp() {
  // Arrange to run `task_environment_` until no components depend on it.
  task_runner_ = new AutoThreadTaskRunner(
      task_environment_.GetMainThreadTaskRunner(), run_loop_.QuitClosure());

  desktop_environment_options_ = DesktopEnvironmentOptions::CreateDefault();

  initial_local_policies_.maximum_session_duration = base::Hours(10);
  local_session_policies_provider_.set_local_policies(initial_local_policies_);

  // Suppress spammy "uninteresting call" logs.
  EXPECT_CALL(client_stub_, SetCursorShape(_)).Times(testing::AnyNumber());
}

void ClientSessionTest::TearDown() {
  if (client_session_) {
    if (client_session_->is_authenticated()) {
      client_session_->DisconnectSession(ErrorCode::OK, {}, FROM_HERE);
    }
    mock_peer_session_ = nullptr;
    client_session_.reset();
    session_ = nullptr;
  }

  // Clear out `task_runner_` reference so the loop can quit, and run it until
  // it does.
  task_runner_ = nullptr;
  run_loop_.Run();
}

void ClientSessionTest::CreateClientSession(
    std::unique_ptr<protocol::FakeSession> session) {
  DCHECK(session);
  session_ = session.get();

  auto mock_peer = std::make_unique<testing::NiceMock<MockPeerSession>>();
  mock_peer_session_ = mock_peer.get();
  ON_CALL(*mock_peer_session_, DisconnectSession(_, _, _))
      .WillByDefault([this](protocol::ErrorCode error,
                            std::string_view error_details,
                            const SourceLocation& error_location) {
        client_session_->OnSessionClosed(error, error_details, error_location);
      });

  EXPECT_CALL(mock_peer_session_factory_, Create())
      .Times(AtMost(1))
      .WillOnce(Return(ByMove(std::move(mock_peer))));

  client_session_ = std::make_unique<ClientSession>(
      &session_event_handler_, std::move(session), &mock_peer_session_factory_,
      desktop_environment_options_, extensions_,
      &local_session_policies_provider_);
}

void ClientSessionTest::CreateClientSession() {
  CreateClientSession(std::make_unique<protocol::FakeSession>());
}

void ClientSessionTest::AuthenticateClientSession(
    const SessionPolicies* session_policies) {
  client_session_->OnConnectionAuthenticated(session_policies);
}

void ClientSessionTest::ConnectClientSession(
    const SessionPolicies* session_policies) {
  EXPECT_CALL(session_event_handler_, OnSessionPoliciesReceived(_))
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(session_event_handler_, OnSessionAuthenticated(_));

  base::test::TestFuture<void> future;
  EXPECT_CALL(session_event_handler_, OnSessionChannelsConnected(_))
      .WillOnce([&future] { future.SetValue(); });

  AuthenticateClientSession(session_policies);
  client_session_->OnSessionChannelsConnected();
  future.Get();
}

TEST_F(ClientSessionTest,
       EffectivePoliciesImplicitlyAllowFileTransfer_HasCapability) {
  local_session_policies_provider_.set_local_policies({});
  CreateClientSession();

  EXPECT_CALL(
      *mock_peer_session_,
      Start(_, _, _, _,
            testing::Field(&SessionPolicies::allow_file_transfer, std::nullopt),
            _));

  ConnectClientSession();
}

TEST_F(ClientSessionTest,
       EffectivePoliciesExplicitlyAllowFileTransfer_HasCapability) {
  SessionPolicies local_policies;
  local_policies.allow_file_transfer = true;
  local_session_policies_provider_.set_local_policies(local_policies);
  CreateClientSession();

  EXPECT_CALL(*mock_peer_session_,
              Start(_, _, _, _,
                    testing::Field(&SessionPolicies::allow_file_transfer,
                                   std::optional<bool>(true)),
                    _));

  ConnectClientSession();
}

TEST_F(ClientSessionTest,
       EffectivePoliciesDisallowFileTransfer_DoesNotHaveCapability) {
  SessionPolicies local_policies;
  local_policies.allow_file_transfer = false;
  local_session_policies_provider_.set_local_policies(local_policies);
  CreateClientSession();

  EXPECT_CALL(*mock_peer_session_,
              Start(_, _, _, _,
                    testing::Field(&SessionPolicies::allow_file_transfer,
                                   std::optional<bool>(false)),
                    _));

  ConnectClientSession();
}

TEST_F(ClientSessionTest, ApplyPoliciesFromRemotePolicies) {
  SessionPolicies local_policies;
  local_policies.allow_file_transfer = true;
  local_policies.allow_uri_forwarding = true;
  local_session_policies_provider_.set_local_policies(local_policies);
  SessionPolicies remote_policies;
  remote_policies.allow_file_transfer = false;
  remote_policies.allow_uri_forwarding = false;

  CreateClientSession();

  EXPECT_CALL(*mock_peer_session_,
              Start(_, _, _, _,
                    testing::AllOf(
                        testing::Field(&SessionPolicies::allow_file_transfer,
                                       std::optional<bool>(false)),
                        testing::Field(&SessionPolicies::allow_uri_forwarding,
                                       std::optional<bool>(false))),
                    _));

  ConnectClientSession(&remote_policies);
}

TEST_F(ClientSessionTest, ForwardHostSessionOptions1) {
  auto session = std::make_unique<protocol::FakeSession>();
  Attachment attachment;
  attachment.host_config.emplace();
  attachment.host_config->settings[kSessionOptionDetectUpdatedRegion] = "true";
  session->SetAttachment(0, attachment);

  CreateClientSession(std::move(session));

  SessionOptions options;
  EXPECT_CALL(*mock_peer_session_, Start(_, _, _, _, _, _))
      .WillOnce(testing::SaveArg<5>(&options));

  ConnectClientSession();
  EXPECT_EQ(options.detect_updated_region, true);
}

TEST_F(ClientSessionTest, ForwardHostSessionOptions2) {
  auto session = std::make_unique<protocol::FakeSession>();
  Attachment attachment;
  attachment.host_config.emplace();
  attachment.host_config->settings[kSessionOptionDetectUpdatedRegion] = "false";
  session->SetAttachment(0, attachment);

  CreateClientSession(std::move(session));

  SessionOptions options;
  EXPECT_CALL(*mock_peer_session_, Start(_, _, _, _, _, _))
      .WillOnce(testing::SaveArg<5>(&options));

  ConnectClientSession();
  EXPECT_EQ(options.detect_updated_region, false);
}

TEST_F(ClientSessionTest, ForwardHostSessionOptionsAllFields) {
  auto session = std::make_unique<protocol::FakeSession>();
  Attachment attachment;
  attachment.host_config.emplace();
  attachment.host_config->settings[kSessionOptionDetectUpdatedRegion] = "true";
  attachment.host_config
      ->settings[kSessionOptionCaptureVideoOnDedicatedThread] = "false";
  attachment.host_config->settings[kSessionOptionDisableUdp] = "true";
  attachment.host_config->settings[kSessionOptionVp9EncoderSpeed] = "3";
  attachment.host_config->settings[kSessionOptionAv1ActiveMap] = "1";
  attachment.host_config->settings[kSessionOptionAv1EncoderSpeed] = "4";
  session->SetAttachment(0, attachment);

  CreateClientSession(std::move(session));

  SessionOptions options;
  EXPECT_CALL(*mock_peer_session_, Start(_, _, _, _, _, _))
      .WillOnce(testing::SaveArg<5>(&options));

  ConnectClientSession();
  EXPECT_EQ(options.detect_updated_region, true);
  EXPECT_EQ(options.capture_video_on_dedicated_thread, false);
  EXPECT_EQ(options.disable_udp, true);
  EXPECT_EQ(options.vp9_encoder_speed, 3);
  EXPECT_EQ(options.av1_active_map, true);
  EXPECT_EQ(options.av1_encoder_speed, 4);
}

TEST_F(ClientSessionTest, ForwardHostSessionOptionsIgnoresUnsupportedKey) {
  auto session = std::make_unique<protocol::FakeSession>();
  Attachment attachment;
  attachment.host_config.emplace();
  attachment.host_config->settings[kSessionOptionDetectUpdatedRegion] = "true";
  attachment.host_config->settings["Unsupported-Key"] = "foo";
  session->SetAttachment(0, attachment);

  CreateClientSession(std::move(session));

  SessionOptions options;
  EXPECT_CALL(*mock_peer_session_, Start(_, _, _, _, _, _))
      .WillOnce(testing::SaveArg<5>(&options));

  ConnectClientSession();
  EXPECT_EQ(options.detect_updated_region, true);
}

TEST_F(
    ClientSessionTest,
    OnLocalPoliciesChanged_DoesNotDisconnectIfEffectivePoliciesComeFromRemotePolicies) {
  SessionPolicies remote_policies;
  remote_policies.maximum_session_duration = base::Hours(8);
  CreateClientSession();
  ConnectClientSession(&remote_policies);

  EXPECT_TRUE(client_session_->is_authenticated());
  SessionPolicies new_policies;
  new_policies.maximum_session_duration = base::Hours(23);
  local_session_policies_provider_.set_local_policies(new_policies);
  EXPECT_TRUE(client_session_->is_authenticated());
}

TEST_F(ClientSessionTest,
       OnLocalPoliciesChanged_DoesNotDisconnectIfEffectivePoliciesNotChanged) {
  CreateClientSession();
  ConnectClientSession();

  EXPECT_TRUE(client_session_->is_authenticated());
  local_session_policies_provider_.set_local_policies(initial_local_policies_);
  EXPECT_TRUE(client_session_->is_authenticated());
}

TEST_F(ClientSessionTest,
       OnLocalPoliciesChanged_DisconnectsIfEffectivePoliciesChanged) {
  CreateClientSession();
  ConnectClientSession();

  EXPECT_TRUE(client_session_->is_authenticated());
  EXPECT_CALL(*mock_peer_session_,
              DisconnectSession(ErrorCode::SESSION_POLICIES_CHANGED, _, _));
  EXPECT_CALL(*mock_peer_session_, DisconnectSession(ErrorCode::OK, _, _))
      .Times(testing::AnyNumber());

  SessionPolicies local_policies;
  local_policies.maximum_session_duration = base::Hours(23);
  local_session_policies_provider_.set_local_policies(local_policies);
}

TEST_F(ClientSessionTest, DisconnectsIfOnSessionPoliciesReceivedReturnsError) {
  EXPECT_CALL(session_event_handler_,
              OnSessionPoliciesReceived(initial_local_policies_))
      .WillOnce(Return(ErrorCode::DISALLOWED_BY_POLICY));

  CreateClientSession();
  AuthenticateClientSession(nullptr);

  EXPECT_FALSE(client_session_->is_authenticated());
  EXPECT_EQ(session_->error(), ErrorCode::DISALLOWED_BY_POLICY);
}

}  // namespace remoting
