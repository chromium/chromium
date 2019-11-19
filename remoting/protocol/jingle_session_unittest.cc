// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_session.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "net/socket/socket.h"
#include "net/socket/stream_socket.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
#include "remoting/protocol/connection_tester.h"
#include "remoting/protocol/fake_authenticator.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/session_plugin.h"
#include "remoting/protocol/transport.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmpp/constants.h"

using testing::_;
using testing::AtLeast;
using testing::AtMost;
using testing::DeleteArg;
using testing::DoAll;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SaveArg;
using testing::SetArgPointee;
using testing::WithArg;

namespace remoting {
namespace protocol {

namespace {

const char kHostJid[] = "Host@gmail.com/123";
const char kClientJid[] = "Client@gmail.com/321";

// kHostJid the way it would be stored in the directory.
const char kNormalizedHostJid[] = "host@gmail.com/123";

class MockSessionManagerListener {
 public:
  MOCK_METHOD2(OnIncomingSession,
               void(Session*,
                    SessionManager::IncomingSessionResponse*));
};

class MockSessionEventHandler : public Session::EventHandler {
 public:
  MOCK_METHOD1(OnSessionStateChange, void(Session::State));
  MOCK_METHOD2(OnSessionRouteChange, void(const std::string& channel_name,
                                          const TransportRoute& route));
};

class FakeTransport : public Transport {
 public:
  SendTransportInfoCallback send_transport_info_callback() {
    return send_transport_info_callback_;
  }

  const std::vector<std::unique_ptr<jingle_xmpp::XmlElement>>& received_messages() {
    return received_messages_;
  }

  void set_on_message_callback(const base::Closure& on_message_callback) {
    on_message_callback_ = on_message_callback;
  }

  // Transport interface.
  void Start(Authenticator* authenticator,
             SendTransportInfoCallback send_transport_info_callback) override {
    send_transport_info_callback_ = send_transport_info_callback;
  }

  bool ProcessTransportInfo(jingle_xmpp::XmlElement* transport_info) override {
    received_messages_.push_back(
        std::make_unique<jingle_xmpp::XmlElement>(*transport_info));
    if (!on_message_callback_.is_null())
      on_message_callback_.Run();
    return true;
  }

 private:
  SendTransportInfoCallback send_transport_info_callback_;
  std::vector<std::unique_ptr<jingle_xmpp::XmlElement>> received_messages_;
  base::Closure on_message_callback_;
};

class FakePlugin : public SessionPlugin {
 public:
   std::unique_ptr<jingle_xmpp::XmlElement> GetNextMessage() override {
     std::string tag_name = "test-tag-";
     tag_name += base::NumberToString(outgoing_messages_.size());
     std::unique_ptr<jingle_xmpp::XmlElement> new_message(new jingle_xmpp::XmlElement(
         jingle_xmpp::QName("test-namespace", tag_name)));
     outgoing_messages_.push_back(*new_message);
     return new_message;
  }

  void OnIncomingMessage(const jingle_xmpp::XmlElement& attachments) override {
    for (const jingle_xmpp::XmlElement* it = attachments.FirstElement();
         it != nullptr;
         it = it->NextElement()) {
      incoming_messages_.push_back(*it);
    }
  }

  const std::vector<jingle_xmpp::XmlElement>& outgoing_messages() const {
    return outgoing_messages_;
  }

  const std::vector<jingle_xmpp::XmlElement>& incoming_messages() const {
    return incoming_messages_;
  }

  void Clear() {
    outgoing_messages_.clear();
    incoming_messages_.clear();
  }

 private:
  std::vector<jingle_xmpp::XmlElement> outgoing_messages_;
  std::vector<jingle_xmpp::XmlElement> incoming_messages_;
};

std::unique_ptr<jingle_xmpp::XmlElement> CreateTransportInfo(const std::string& id) {
  std::unique_ptr<jingle_xmpp::XmlElement> result(
      jingle_xmpp::XmlElement::ForStr("<transport xmlns='google:remoting:ice'/>"));
  result->AddAttr(jingle_xmpp::QN_ID, id);
  return result;
}

}  // namespace

class JingleSessionTest : public testing::Test {
 public:
  JingleSessionTest() {
    network_settings_ =
        NetworkSettings(NetworkSettings::NAT_TRAVERSAL_OUTGOING);
  }

  // Helper method that handles OnIncomingSession().
  void SetHostSession(Session* session) {
    DCHECK(session);
    host_session_.reset(session);
    host_session_->SetEventHandler(&host_session_event_handler_);
    host_session_->SetTransport(&host_transport_);
    host_session_->AddPlugin(&host_plugin_);
  }

  void DeleteHostSession() { host_session_.reset(); }

  void DeleteClientSession() { client_session_.reset(); }

 protected:
  void TearDown() override {
    CloseSessions();
    CloseSessionManager();
    base::RunLoop().RunUntilIdle();
  }

  void CloseSessions() {
    host_session_.reset();
    client_session_.reset();
  }

  void CreateSessionManagers(FakeAuthenticator::Config auth_config,
                             int messages_till_start) {
    host_signal_strategy_ =
        std::make_unique<FakeSignalStrategy>(SignalingAddress(kHostJid));
    client_signal_strategy_ =
        std::make_unique<FakeSignalStrategy>(SignalingAddress(kClientJid));

    FakeSignalStrategy::Connect(host_signal_strategy_.get(),
                                client_signal_strategy_.get());

    host_server_.reset(new JingleSessionManager(host_signal_strategy_.get()));
    host_server_->AcceptIncoming(
        base::Bind(&MockSessionManagerListener::OnIncomingSession,
                   base::Unretained(&host_server_listener_)));

    std::unique_ptr<AuthenticatorFactory> factory(
        new FakeHostAuthenticatorFactory(messages_till_start, auth_config));
    host_server_->set_authenticator_factory(std::move(factory));

    client_server_.reset(
        new JingleSessionManager(client_signal_strategy_.get()));
  }

  void CreateSessionManagers(FakeAuthenticator::Config auth_config) {
    CreateSessionManagers(auth_config, 0);
  }

  void CloseSessionManager() {
    host_server_.reset();
    client_server_.reset();
    host_signal_strategy_.reset();
    client_signal_strategy_.reset();
  }

  void SetHostExpectation(bool expect_fail) {
    EXPECT_CALL(host_server_listener_, OnIncomingSession(_, _))
        .WillOnce(
            DoAll(WithArg<0>(Invoke(this, &JingleSessionTest::SetHostSession)),
                  SetArgPointee<1>(protocol::SessionManager::ACCEPT)));

    {
      InSequence dummy;

      EXPECT_CALL(host_session_event_handler_,
                  OnSessionStateChange(Session::ACCEPTED))
          .Times(AtMost(1));
      EXPECT_CALL(host_session_event_handler_,
                  OnSessionStateChange(Session::AUTHENTICATING))
          .Times(AtMost(1));
      if (expect_fail) {
        EXPECT_CALL(host_session_event_handler_,
                    OnSessionStateChange(Session::FAILED))
            .Times(1);
      } else {
        EXPECT_CALL(host_session_event_handler_,
                    OnSessionStateChange(Session::AUTHENTICATED))
            .Times(1);
        // Expect that the connection will be closed eventually.
        EXPECT_CALL(host_session_event_handler_,
                    OnSessionStateChange(Session::CLOSED))
            .Times(AtMost(1));
      }
    }
  }

  void SetClientExpectation(bool expect_fail) {
    InSequence dummy;

    EXPECT_CALL(client_session_event_handler_,
                OnSessionStateChange(Session::ACCEPTED))
        .Times(AtMost(1));
    EXPECT_CALL(client_session_event_handler_,
                OnSessionStateChange(Session::AUTHENTICATING))
        .Times(AtMost(1));
    if (expect_fail) {
      EXPECT_CALL(client_session_event_handler_,
                  OnSessionStateChange(Session::FAILED))
          .Times(1);
    } else {
      EXPECT_CALL(client_session_event_handler_,
                  OnSessionStateChange(Session::AUTHENTICATED))
          .Times(1);
      // Expect that the connection will be closed eventually.
      EXPECT_CALL(client_session_event_handler_,
                  OnSessionStateChange(Session::CLOSED))
          .Times(AtMost(1));
    }
  }

  void ConnectClient(std::unique_ptr<Authenticator> authenticator) {
    client_session_ = client_server_->Connect(
        SignalingAddress(kNormalizedHostJid), std::move(authenticator));
    client_session_->SetEventHandler(&client_session_event_handler_);
    client_session_->SetTransport(&client_transport_);
    client_session_->AddPlugin(&client_plugin_);
    base::RunLoop().RunUntilIdle();
  }

  void ConnectClient(FakeAuthenticator::Config auth_config) {
    ConnectClient(std::make_unique<FakeAuthenticator>(
        FakeAuthenticator::CLIENT, auth_config,
        client_signal_strategy_->GetLocalAddress().id(), kNormalizedHostJid));
  }

  void InitiateConnection(FakeAuthenticator::Config auth_config,
                          bool expect_fail) {
    SetHostExpectation(expect_fail);
    SetClientExpectation(expect_fail);
    ConnectClient(auth_config);
  }

  void ExpectRouteChange(const std::string& channel_name) {
    EXPECT_CALL(host_session_event_handler_,
                OnSessionRouteChange(channel_name, _))
        .Times(AtLeast(1));
    EXPECT_CALL(client_session_event_handler_,
                OnSessionRouteChange(channel_name, _))
        .Times(AtLeast(1));
  }

  void ExpectPluginMessagesEqual() const {
    ASSERT_EQ(client_plugin_.outgoing_messages().size(),
              host_plugin_.incoming_messages().size());
    for (size_t i = 0; i < client_plugin_.outgoing_messages().size(); i++) {
      ASSERT_EQ(client_plugin_.outgoing_messages()[i].Str(),
                host_plugin_.incoming_messages()[i].Str());
    }

    ASSERT_EQ(client_plugin_.incoming_messages().size(),
              host_plugin_.outgoing_messages().size());
    for (size_t i = 0; i < client_plugin_.incoming_messages().size(); i++) {
      ASSERT_EQ(client_plugin_.incoming_messages()[i].Str(),
                host_plugin_.outgoing_messages()[i].Str());
    }
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  NetworkSettings network_settings_;

  std::unique_ptr<FakeSignalStrategy> host_signal_strategy_;
  std::unique_ptr<FakeSignalStrategy> client_signal_strategy_;

  std::unique_ptr<JingleSessionManager> host_server_;
  MockSessionManagerListener host_server_listener_;
  std::unique_ptr<JingleSessionManager> client_server_;

  std::unique_ptr<Session> host_session_;
  MockSessionEventHandler host_session_event_handler_;
  FakeTransport host_transport_;
  std::unique_ptr<Session> client_session_;
  MockSessionEventHandler client_session_event_handler_;
  FakeTransport client_transport_;

  FakePlugin host_plugin_;
  FakePlugin client_plugin_;
};


// Verify that we can create and destroy session managers without a
// connection.
TEST_F(JingleSessionTest, CreateAndDestoy) {
  CreateSessionManagers(FakeAuthenticator::Config(FakeAuthenticator::ACCEPT));
}

// Verify that an incoming session can be rejected, and that the
// status of the connection is set to FAILED in this case.
TEST_F(JingleSessionTest, RejectConnection) {
  CreateSessionManagers(FakeAuthenticator::Config(FakeAuthenticator::ACCEPT));

  // Reject incoming session.
  EXPECT_CALL(host_server_listener_, OnIncomingSession(_, _))
      .WillOnce(SetArgPointee<1>(protocol::SessionManager::DECLINE));

  {
    InSequence dummy;
    EXPECT_CALL(client_session_event_handler_,
                OnSessionStateChange(Session::FAILED))
        .Times(1);
  }

  ConnectClient(FakeAuthenticator::Config(FakeAuthenticator::ACCEPT));
  client_session_->SetEventHandler(&client_session_event_handler_);

  base::RunLoop().RunUntilIdle();
}

// Verify that we can connect two endpoints with single-step authentication.
TEST_F(JingleSessionTest, Connect) {
  CreateSessionManagers(FakeAuthenticator::Config(FakeAuthenticator::ACCEPT));
  InitiateConnection(FakeAuthenticator::Config(), false);

  // Verify that the client specified correct initiator value.
  ASSERT_GT(host_signal_strategy_->received_messages().size(), 0U);
  const jingle_xmpp::XmlElement* initiate_xml =
      host_signal_strategy_->received_messages().front().get();
  const jingle_xmpp::XmlElement* jingle_element =
      initiate_xml->FirstNamed(jingle_xmpp::QName("urn:xmpp:jingle:1", "jingle"));
  ASSERT_TRUE(jingle_element);
  ASSERT_EQ(client_signal_strategy_->GetLocalAddress().id(),
            jingle_element->Attr(jingle_xmpp::QName(std::string(), "initiator")));
}

// Verify that we can connect two endpoints with multi-step authentication.
TEST_F(JingleSessionTest, ConnectWithMultistep) {
  const int kAuthRoundtrips = 3;
  FakeAuthenticator::Config auth_config(kAuthRoundtrips,
                                        FakeAuthenticator::ACCEPT, true);
  CreateSessionManagers(auth_config);
  InitiateConnection(auth_config, false);
}

TEST_F(JingleSessionTest, ConnectWithOutOfOrderIqs) {
  CreateSessionManagers(FakeAuthenticator::Config(FakeAuthenticator::ACCEPT));
  InitiateConnection(FakeAuthenticator::Config(), false);
  client_signal_strategy_->SimulateMessageReordering();

  // Verify that out of order transport messages are received correctly.
  host_transport_.send_transport_info_callback().Run(CreateTransportInfo("1"));
  host_transport_.send_transport_info_callback().Run(CreateTransportInfo("2"));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(client_transport_.received_messages().size(), 2U);
  EXPECT_EQ("1", client_transport_.received_messages()[0]->Attr(jingle_xmpp::QN_ID));
  EXPECT_EQ("2", client_transport_.received_messages()[1]->Attr(jingle_xmpp::QN_ID));
}

// Verify that out-of-order messages are handled correctly when the session is
// torn down after the first message.
TEST_F(JingleSessionTest, ConnectWithOutOfOrderIqsDestroyOnFirstMessage) {
  CreateSessionManagers(FakeAuthenticator::Config(FakeAuthenticator::ACCEPT));
  InitiateConnection(FakeAuthenticator::Config(), false);
  client_signal_strategy_->SimulateMessageReordering();

  // Verify that out of order transport messages are received correctly.
  host_transport_.send_transport_info_callback().Run(CreateTransportInfo("1"));
  host_transport_.send_transport_info_callback().Run(CreateTransportInfo("2"));

  // Destroy the session as soon as the first message is received.
  client_transport_.set_on_message_callback(base::Bind(
      &JingleSessionTest::DeleteClientSession, base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(client_transport_.received_messages().size(), 1U);
  EXPECT_EQ("1", client_transport_.received_messages()[0]->Attr(jingle_xmpp::QN_ID));
}

// Verify that connection is terminated when single-step auth fails.
TEST_F(JingleSessionTest, ConnectWithBadAuth) {
  CreateSessionManagers(FakeAuthenticator::Config(FakeAuthenticator::REJECT));
  InitiateConnection(FakeAuthenticator::Config(), true);
}

// Verify that connection is terminated when multi-step auth fails.
TEST_F(JingleSessionTest, ConnectWithBadMultistepAuth) {
  const int kAuthRoundtrips = 3;
  CreateSessionManagers(FakeAuthenticator::Config(
      kAuthRoundtrips, FakeAuthenticator::REJECT, false));
  InitiateConnection(FakeAuthenticator::Config(
                         kAuthRoundtrips, FakeAuthenticator::ACCEPT, false),
                     true);
}

// Verify that incompatible protocol configuration is handled properly.
TEST_F(JingleSessionTest, TestIncompatibleProtocol) {
  CreateSessionManagers(FakeAuthenticator::Config(FakeAuthenticator::ACCEPT));

  EXPECT_CALL(host_server_listener_, OnIncomingSession(_, _)).Times(0);

  EXPECT_CALL(client_session_event_handler_,
              OnSessionStateChange(Session::FAILED))
      .Times(1);

  std::unique_ptr<CandidateSessionConfig> config =
      CandidateSessionConfig::CreateDefault();
  // Disable all video codecs so the host will reject connection.
  config->mutable_video_configs()->clear();
  client_server_->set_protocol_config(std::move(config));
  ConnectClient(FakeAuthenticator::Config(FakeAuthenticator::ACCEPT));

  EXPECT_EQ(INCOMPATIBLE_PROTOCOL, client_session_->error());
  EXPECT_FALSE(host_session_);
}

// Verify that GICE-only client is rejected with an appropriate error code.
TEST_F(JingleSessionTest, TestLegacyIceConnection) {
  CreateSessionManagers(FakeAuthenticator::Config(FakeAuthenticator::ACCEPT));

  EXPECT_CALL(host_server_listener_, OnIncomingSession(_, _)).Times(0);

  EXPECT_CALL(client_session_event_handler_,
              OnSessionStateChange(Session::FAILED))
      .Times(1);

  std::unique_ptr<CandidateSessionConfig> config =
      CandidateSessionConfig::CreateDefault();
  config->set_ice_supported(false);
  client_server_->set_protocol_config(std::move(config));
  ConnectClient(FakeAuthenticator::Config(FakeAuthenticator::ACCEPT));

  EXPECT_EQ(INCOMPATIBLE_PROTOCOL, client_session_->error());
  EXPECT_FALSE(host_session_);
}

TEST_F(JingleSessionTest, DeleteSessionOnIncomingConnection) {
  const int kAuthRoundtrips = 3;
  FakeAuthenticator::Config auth_config(kAuthRoundtrips,
                                        FakeAuthenticator::ACCEPT, true);
  CreateSessionManagers(auth_config);

  EXPECT_CALL(host_server_listener_, OnIncomingSession(_, _))
      .WillOnce(
          DoAll(WithArg<0>(Invoke(this, &JingleSessionTest::SetHostSession)),
                SetArgPointee<1>(protocol::SessionManager::ACCEPT)));

  EXPECT_CALL(host_session_event_handler_,
      OnSessionStateChange(Session::ACCEPTED))
      .Times(AtMost(1));

  EXPECT_CALL(host_session_event_handler_,
              OnSessionStateChange(Session::AUTHENTICATING))
      .WillOnce(InvokeWithoutArgs(this, &JingleSessionTest::DeleteHostSession));

  ConnectClient(auth_config);
}

TEST_F(JingleSessionTest, DeleteSessionOnAuth) {
  // Same as the previous test, but set messages_till_started to 2 in
  // CreateSessionManagers so that the session will goes into the
  // AUTHENTICATING state after two message exchanges.
  const int kMessagesTillStarted = 2;

  const int kAuthRoundtrips = 3;
  FakeAuthenticator::Config auth_config(kAuthRoundtrips,
                                        FakeAuthenticator::ACCEPT, true);
  CreateSessionManagers(auth_config, kMessagesTillStarted);

  EXPECT_CALL(host_server_listener_, OnIncomingSession(_, _))
      .WillOnce(
          DoAll(WithArg<0>(Invoke(this, &JingleSessionTest::SetHostSession)),
                SetArgPointee<1>(protocol::SessionManager::ACCEPT)));

  EXPECT_CALL(host_session_event_handler_,
      OnSessionStateChange(Session::ACCEPTED))
      .Times(AtMost(1));

  EXPECT_CALL(host_session_event_handler_,
              OnSessionStateChange(Session::AUTHENTICATING))
      .WillOnce(InvokeWithoutArgs(this, &JingleSessionTest::DeleteHostSession));

  ConnectClient(auth_config);
}

// Verify that incoming transport-info messages are handled correctly while in
// AUTHENTICATING state.
TEST_F(JingleSessionTest, TransportInfoDuringAuthentication) {
  const int kAuthRoundtrips = 2;
  FakeAuthenticator::Config auth_config(kAuthRoundtrips,
                                        FakeAuthenticator::ACCEPT, true);

  CreateSessionManagers(auth_config);

  SetHostExpectation(false);
  {
    InSequence dummy;

    EXPECT_CALL(client_session_event_handler_,
                OnSessionStateChange(Session::ACCEPTED))
        .Times(AtMost(1));
    EXPECT_CALL(client_session_event_handler_,
                OnSessionStateChange(Session::AUTHENTICATING))
        .Times(AtMost(1));
  }

  // Create connection and pause it before authentication is finished.
  FakeAuthenticator* authenticator = new FakeAuthenticator(
      FakeAuthenticator::CLIENT, auth_config,
      client_signal_strategy_->GetLocalAddress().id(), kNormalizedHostJid);
  authenticator->set_pause_message_index(4);
  ConnectClient(base::WrapUnique(authenticator));

  // Send 2 transport messages.
  host_transport_.send_transport_info_callback().Run(CreateTransportInfo("1"));
  host_transport_.send_transport_info_callback().Run(CreateTransportInfo("2"));

  base::RunLoop().RunUntilIdle();

  // The transport-info messages should not be received here because
  // authentication hasn't finished.
  EXPECT_TRUE(client_transport_.received_messages().empty());

  // Destroy the session as soon as the first message is received.
  client_transport_.set_on_message_callback(base::Bind(
      &JingleSessionTest::DeleteClientSession, base::Unretained(this)));

  // Resume authentication.
  authenticator->Resume();
  base::RunLoop().RunUntilIdle();

  // Verify that transport-info that the first transport-info message was
  // received.
  ASSERT_EQ(client_transport_.received_messages().size(), 1U);
  EXPECT_EQ("1", client_transport_.received_messages()[0]->Attr(jingle_xmpp::QN_ID));
}

TEST_F(JingleSessionTest, TestSessionPlugin) {
  host_plugin_.Clear();
  client_plugin_.Clear();

  const int kAuthRoundtrips = 3;
  FakeAuthenticator::Config auth_config(kAuthRoundtrips,
                                        FakeAuthenticator::ACCEPT, true);

  CreateSessionManagers(auth_config);
  ASSERT_NO_FATAL_FAILURE(InitiateConnection(auth_config, false));
  ExpectPluginMessagesEqual();
}

TEST_F(JingleSessionTest, SessionPluginShouldNotBeInvolvedInSessionTerminate) {
  host_plugin_.Clear();
  client_plugin_.Clear();
  CreateSessionManagers(FakeAuthenticator::Config(FakeAuthenticator::REJECT));
  InitiateConnection(FakeAuthenticator::Config(), true);
  // It's expected the client sends one more plugin message than host, the host
  // won't send plugin message in the SESSION_TERMINATE message.
  ASSERT_EQ(client_plugin_.outgoing_messages().size() - 1,
            client_plugin_.incoming_messages().size());
  ExpectPluginMessagesEqual();
}

TEST_F(JingleSessionTest, ImmediatelyCloseSessionAfterConnect) {
  const int kAuthRoundtrips = 3;
  FakeAuthenticator::Config auth_config(kAuthRoundtrips,
                                        FakeAuthenticator::ACCEPT, true);
  CreateSessionManagers(auth_config);
  client_session_ = client_server_->Connect(
      SignalingAddress(kNormalizedHostJid),
      std::make_unique<FakeAuthenticator>(
          FakeAuthenticator::CLIENT, auth_config,
          client_signal_strategy_->GetLocalAddress().id(), kNormalizedHostJid));

  client_session_->Close(HOST_OVERLOAD);
  base::RunLoop().RunUntilIdle();
  // We should only send a SESSION_TERMINATE message if the session has been
  // closed before SESSION_INITIATE message.
  ASSERT_EQ(1U, host_signal_strategy_->received_messages().size());
}

}  // namespace protocol
}  // namespace remoting
