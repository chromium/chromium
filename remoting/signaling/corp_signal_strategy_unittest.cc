// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/corp_signal_strategy.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "remoting/base/http_status.h"
#include "remoting/base/protobuf_http_test_responder.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/proto/messaging_service.h"
#include "remoting/signaling/messaging_client.h"
#include "remoting/signaling/signaling_address.h"
#include "remoting/signaling/xmpp_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

namespace {

using testing::_;
using testing::ByMove;
using testing::Mock;
using testing::Return;

constexpr char kFakeLocalCorpId[] = "fake_local_user@domain.com";
constexpr char kFakeRemoteCorpId[] = "fake_remote_user@domain.com";

enum class Direction {
  OUTGOING,
  INCOMING,
};

std::unique_ptr<jingle_xmpp::XmlElement> CreateXmlStanza(
    Direction direction,
    const std::string& id) {
  static constexpr char kStanzaTemplate[] =
      "<iq xmlns=\"jabber:client\" type=\"set\">"
      "<bind xmlns=\"urn:ietf:params:xml:ns:xmpp-bind\">"
      "<resource>chromoting</resource>"
      "</bind>"
      "</iq>";
  auto stanza = base::WrapUnique<jingle_xmpp::XmlElement>(
      jingle_xmpp::XmlElement::ForStr(kStanzaTemplate));
  stanza->SetAttr(kQNameId, id);
  if (direction == Direction::OUTGOING) {
    stanza->SetAttr(kQNameFrom, kFakeLocalCorpId);
    stanza->SetAttr(kQNameTo, kFakeRemoteCorpId);
  } else {
    stanza->SetAttr(kQNameFrom, kFakeRemoteCorpId);
    stanza->SetAttr(kQNameTo, kFakeLocalCorpId);
  }
  return stanza;
}

class FakeMessagingClient : public MessagingClient {
 public:
  FakeMessagingClient() = default;
  ~FakeMessagingClient() override = default;

  // MessagingClient implementation.
  base::CallbackListSubscription RegisterMessageCallback(
      const MessageCallback& callback) override {
    return callback_list_.Add(callback);
  }

  MOCK_METHOD(void,
              SendMessage,
              (const SignalingAddress&, SignalingMessage&&, DoneCallback),
              (override));
  MOCK_METHOD(void,
              StartReceivingMessages,
              (base::OnceClosure on_ready, DoneCallback on_closed),
              (override));
  MOCK_METHOD(void, StopReceivingMessages, (), (override));
  MOCK_METHOD(bool, IsReceivingMessages, (), (const, override));

  void OnMessage(const SignalingAddress& sender_address,
                 const SignalingMessage& message) {
    callback_list_.Notify(sender_address, message);
  }

 private:
  MessageCallbackList callback_list_;
};

}  // namespace

class CorpSignalStrategyTest : public testing::Test,
                               public SignalStrategy::Listener {
 public:
  CorpSignalStrategyTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    auto messaging_client = std::make_unique<FakeMessagingClient>();
    messaging_client_ = messaging_client.get();

    signal_strategy_ =
        std::unique_ptr<CorpSignalStrategy>(new CorpSignalStrategy(
            std::move(messaging_client), SignalingAddress(kFakeLocalCorpId)));
    signal_strategy_->AddListener(this);

    ON_CALL(*this, OnSignalStrategyIncomingMessage(_, _))
        .WillByDefault(Return(false));
  }

  ~CorpSignalStrategyTest() override {
    signal_strategy_->RemoveListener(this);
    signal_strategy_.reset();
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  MOCK_METHOD(bool,
              OnSignalStrategyIncomingMessage,
              (const SignalingAddress&, const SignalingMessage&),
              (override));

  base::test::TaskEnvironment task_environment_;
  ProtobufHttpTestResponder test_responder_;

  raw_ptr<FakeMessagingClient, AcrossTasksDanglingUntriaged> messaging_client_ =
      nullptr;
  std::unique_ptr<CorpSignalStrategy> signal_strategy_;

  std::vector<SignalStrategy::State> state_history_;
  std::vector<std::unique_ptr<jingle_xmpp::XmlElement>> received_messages_;

 private:
  // SignalStrategy::Listener overrides.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override {
    state_history_.push_back(state);
  }

  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override {
    received_messages_.push_back(
        std::make_unique<jingle_xmpp::XmlElement>(*stanza));
    return true;
  }
};

TEST_F(CorpSignalStrategyTest, ConnectAndDisconnect) {
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::OK, signal_strategy_->GetError());
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce([](base::OnceClosure on_ready,
                   MessagingClient::DoneCallback on_closed) {
        std::move(on_ready).Run();
        return testing::Return();
      });
  ASSERT_EQ(0u, state_history_.size());
  signal_strategy_->Connect();

  ASSERT_EQ(2u, state_history_.size());
  ASSERT_EQ(SignalStrategy::State::CONNECTING, state_history_[0]);
  ASSERT_EQ(SignalStrategy::State::CONNECTED, state_history_[1]);

  ASSERT_EQ(SignalStrategy::State::CONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::OK, signal_strategy_->GetError());
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  EXPECT_CALL(*messaging_client_, StopReceivingMessages());
  signal_strategy_->Disconnect();

  ASSERT_EQ(3u, state_history_.size());
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, state_history_[2]);
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
}

TEST_F(CorpSignalStrategyTest, StartStream_Unauthenticated) {
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::OK, signal_strategy_->GetError());
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce([](base::OnceClosure on_ready,
                   MessagingClient::DoneCallback on_closed) {
        std::move(on_closed).Run(
            HttpStatus(HttpStatus::Code::UNAUTHENTICATED, "unauthenticated"));
      });

  signal_strategy_->Connect();

  ASSERT_EQ(2u, state_history_.size());
  ASSERT_EQ(SignalStrategy::State::CONNECTING, state_history_[0]);
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, state_history_[1]);

  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::AUTHENTICATION_FAILED,
            signal_strategy_->GetError());
  ASSERT_TRUE(signal_strategy_->IsSignInError());
}

TEST_F(CorpSignalStrategyTest, StartStream_NetworkError) {
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::OK, signal_strategy_->GetError());
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce([](base::OnceClosure on_ready,
                   MessagingClient::DoneCallback on_closed) {
        std::move(on_closed).Run(
            HttpStatus(HttpStatus::Code::UNAVAILABLE, "unavailable"));
      });

  signal_strategy_->Connect();

  ASSERT_EQ(2u, state_history_.size());
  ASSERT_EQ(SignalStrategy::State::CONNECTING, state_history_[0]);
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, state_history_[1]);

  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::NETWORK_ERROR, signal_strategy_->GetError());
  ASSERT_FALSE(signal_strategy_->IsSignInError());
}

TEST_F(CorpSignalStrategyTest, SendStanza_Success) {
  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce([](base::OnceClosure on_ready,
                   MessagingClient::DoneCallback on_closed) {
        std::move(on_ready).Run();
        return testing::Return();
      });
  signal_strategy_->Connect();

  // Simulate an incoming message to set the remote address.
  internal::IqStanzaStruct iq_stanza_struct;
  iq_stanza_struct.xml = CreateXmlStanza(Direction::INCOMING, "id1")->Str();
  iq_stanza_struct.messaging_authz_token = "faux_messaging_token";
  internal::PeerMessageStruct peer_message;
  peer_message.payload = std::move(iq_stanza_struct);
  messaging_client_->OnMessage(SignalingAddress(kFakeRemoteCorpId),
                               SignalingMessage(peer_message));

  auto stanza =
      CreateXmlStanza(Direction::OUTGOING, signal_strategy_->GetNextId());
  std::string stanza_string = stanza->Str();

  EXPECT_CALL(*messaging_client_, SendMessage(_, _, _))
      .WillOnce([stanza_string](const SignalingAddress& address,
                                SignalingMessage&& message,
                                MessagingClient::DoneCallback on_done) {
        EXPECT_EQ("faux_messaging_token", address.id());
        auto* peer_message = std::get_if<internal::PeerMessageStruct>(&message);
        ASSERT_TRUE(peer_message);
        auto* iq_stanza =
            std::get_if<internal::IqStanzaStruct>(&peer_message->payload);
        ASSERT_TRUE(iq_stanza);
        EXPECT_EQ(stanza_string, iq_stanza->xml);
        std::move(on_done).Run(HttpStatus::OK());
      });

  signal_strategy_->SendStanza(std::move(stanza));
}

TEST_F(CorpSignalStrategyTest, SendStanza_NotConnected) {
  auto stanza =
      CreateXmlStanza(Direction::OUTGOING, signal_strategy_->GetNextId());
  EXPECT_FALSE(signal_strategy_->SendStanza(std::move(stanza)));
}

TEST_F(CorpSignalStrategyTest, ReceiveStanza_Success) {
  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce([](base::OnceClosure on_ready,
                   MessagingClient::DoneCallback on_closed) {
        std::move(on_ready).Run();
        return testing::Return();
      });
  signal_strategy_->Connect();

  auto stanza =
      CreateXmlStanza(Direction::INCOMING, signal_strategy_->GetNextId());
  std::string stanza_string = stanza->Str();

  internal::IqStanzaStruct iq_stanza_struct;
  iq_stanza_struct.xml = stanza_string;
  iq_stanza_struct.messaging_authz_token = "fake_authz_token";

  internal::PeerMessageStruct peer_message;
  peer_message.payload = std::move(iq_stanza_struct);

  messaging_client_->OnMessage(SignalingAddress(kFakeRemoteCorpId),
                               SignalingMessage(peer_message));

  ASSERT_EQ(1u, received_messages_.size());
  ASSERT_EQ(stanza_string, received_messages_[0]->Str());
}

TEST_F(CorpSignalStrategyTest, ReceiveStanza_MalformedXmpp) {
  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce([](base::OnceClosure on_ready,
                   MessagingClient::DoneCallback on_closed) {
        std::move(on_ready).Run();
        return testing::Return();
      });
  signal_strategy_->Connect();

  internal::IqStanzaStruct iq_stanza_struct;
  iq_stanza_struct.xml = "Malformed!!!";

  internal::PeerMessageStruct peer_message;
  peer_message.payload = std::move(iq_stanza_struct);

  messaging_client_->OnMessage(SignalingAddress(kFakeRemoteCorpId),
                               SignalingMessage(peer_message));

  ASSERT_EQ(0u, received_messages_.size());
}

}  // namespace remoting
