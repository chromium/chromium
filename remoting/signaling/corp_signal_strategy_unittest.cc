// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/corp_signal_strategy.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "remoting/base/http_status.h"
#include "remoting/base/internal_headers.h"
#include "remoting/base/protobuf_http_test_responder.h"
#include "remoting/signaling/corp_messaging_client.h"
#include "remoting/signaling/jingle_message_xml_converter.h"
#include "remoting/signaling/signaling_address.h"
#include "remoting/signaling/xmpp_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

namespace {

using testing::_;
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
      "<jingle xmlns=\"urn:xmpp:jingle:1\" action=\"session-info\" "
      "sid=\"sid123\">"
      "<rem:test-info xmlns:rem=\"google:remoting\">TestMessage</rem:test-info>"
      "</jingle>"
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

class FakeMessagingClient : public CorpMessagingClient {
 public:
  FakeMessagingClient() = default;
  ~FakeMessagingClient() override = default;

  // CorpMessagingClient implementation.
  base::CallbackListSubscription RegisterMessageCallback(
      const CorpMessagingClient::MessageCallback& callback) override {
    return callback_list_.Add(callback);
  }

  MOCK_METHOD(void,
              SendMessage,
              (const SignalingAddress&,
               internal::PeerMessageStruct&&,
               CorpMessagingClient::DoneCallback),
              (override));
  MOCK_METHOD(void,
              StartReceivingMessages,
              (base::OnceClosure on_ready,
               CorpMessagingClient::DoneCallback on_closed),
              (override));
  MOCK_METHOD(void, StopReceivingMessages, (), (override));
  MOCK_METHOD(bool, IsReceivingMessages, (), (const, override));

  void OnMessage(const SignalingAddress& sender_address,
                 const internal::PeerMessageStruct& message) {
    callback_list_.Notify(sender_address, message);
  }

 private:
  CorpMessagingClient::MessageCallbackList callback_list_;
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

    // By default, messages will be collected in received_messages_.
    ON_CALL(*this, OnSignalingMessage(_, _))
        .WillByDefault([&](const SignalingAddress& sender_address,
                           const JingleMessage& jingle_message) {
          received_messages_.push_back(JingleMessageToXml(jingle_message));
          return true;
        });
    ON_CALL(*this, OnSignalingReply(_, _))
        .WillByDefault([&](const SignalingAddress& sender_address,
                           const JingleMessageReply& jingle_reply) {
          received_messages_.push_back(JingleMessageReplyToXml(jingle_reply));
          return true;
        });
  }

  ~CorpSignalStrategyTest() override {
    signal_strategy_->RemoveListener(this);
    signal_strategy_.reset();
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  MOCK_METHOD(bool,
              OnSignalingMessage,
              (const SignalingAddress&, const JingleMessage&),
              (override));

  MOCK_METHOD(bool,
              OnSignalingReply,
              (const SignalingAddress&, const JingleMessageReply&),
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
  void OnSignalingStateChanged(SignalStrategy::State state) override {
    state_history_.push_back(state);
  }
};

TEST_F(CorpSignalStrategyTest, ConnectAndDisconnect) {
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::OK, signal_strategy_->GetError());
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce([](base::OnceClosure on_ready,
                   CorpMessagingClient::DoneCallback on_closed) {
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
                   CorpMessagingClient::DoneCallback on_closed) {
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
                   CorpMessagingClient::DoneCallback on_closed) {
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

TEST_F(CorpSignalStrategyTest, SendMessage_XmlElement_Success) {
  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce([](base::OnceClosure on_ready,
                   CorpMessagingClient::DoneCallback on_closed) {
        std::move(on_ready).Run();
        return testing::Return();
      });
  signal_strategy_->Connect();

  // Simulate an incoming message to set the remote address.
  internal::IqStanzaStruct iq_stanza_struct;
  iq_stanza_struct.xml = CreateXmlStanza(Direction::INCOMING, "id1")->Str();
  iq_stanza_struct.messaging_authz_token = "faux_messaging_token";
  internal::PeerMessageStruct message;
  message.payload = std::move(iq_stanza_struct);
  messaging_client_->OnMessage(SignalingAddress(kFakeRemoteCorpId), message);

  auto stanza =
      CreateXmlStanza(Direction::OUTGOING, signal_strategy_->GetNextId());
  std::string stanza_string = stanza->Str();

  JingleMessage jingle_message;
  std::string error;
  ASSERT_TRUE(JingleMessageFromXml(stanza.get(), &jingle_message, &error));

  EXPECT_CALL(*messaging_client_, SendMessage(_, _, _))
      .WillOnce([&](const SignalingAddress& address,
                    internal::PeerMessageStruct&& message,
                    CorpMessagingClient::DoneCallback on_done) {
        EXPECT_EQ("faux_messaging_token", address.id());
        auto* iq_stanza =
            std::get_if<internal::IqStanzaStruct>(&message.payload);
        ASSERT_TRUE(iq_stanza);
        EXPECT_THAT(iq_stanza->xml,
                    testing::HasSubstr("to=\"fake_remote_user@domain.com\""));
        EXPECT_THAT(iq_stanza->xml,
                    testing::HasSubstr("from=\"fake_local_user@domain.com\""));
        std::move(on_done).Run(HttpStatus::OK());
      });

  signal_strategy_->SendMessage(std::move(jingle_message));
}

TEST_F(CorpSignalStrategyTest, SendMessage_XmlElement_NotConnected) {
  EXPECT_CALL(*messaging_client_, SendMessage(_, _, _)).Times(0);
  auto stanza =
      CreateXmlStanza(Direction::OUTGOING, signal_strategy_->GetNextId());
  JingleMessage jingle_message;
  std::string error;
  ASSERT_TRUE(JingleMessageFromXml(stanza.get(), &jingle_message, &error));

  ASSERT_FALSE(signal_strategy_->SendMessage(std::move(jingle_message)));
}

TEST_F(CorpSignalStrategyTest, ReceiveStanza_Success) {
  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce([](base::OnceClosure on_ready,
                   CorpMessagingClient::DoneCallback on_closed) {
        std::move(on_ready).Run();
        return testing::Return();
      });
  signal_strategy_->Connect();

  auto stanza =
      CreateXmlStanza(Direction::INCOMING, signal_strategy_->GetNextId());

  internal::IqStanzaStruct iq_stanza_struct;
  iq_stanza_struct.xml = stanza->Str();
  iq_stanza_struct.messaging_authz_token = "fake_authz_token";

  internal::PeerMessageStruct message;
  message.payload = std::move(iq_stanza_struct);

  messaging_client_->OnMessage(SignalingAddress(kFakeRemoteCorpId), message);

  ASSERT_EQ(1u, received_messages_.size());
  // The attribute order may change during XML conversion.
  std::string received_stanza_string = received_messages_[0]->Str();
  EXPECT_THAT(received_stanza_string,
              testing::HasSubstr("to=\"fake_local_user@domain.com\""));
  EXPECT_THAT(received_stanza_string,
              testing::HasSubstr("from=\"fake_remote_user@domain.com\""));
}

TEST_F(CorpSignalStrategyTest, ReceiveStanza_MalformedXmpp) {
  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce([](base::OnceClosure on_ready,
                   CorpMessagingClient::DoneCallback on_closed) {
        std::move(on_ready).Run();
        return testing::Return();
      });
  signal_strategy_->Connect();

  internal::IqStanzaStruct iq_stanza_struct;
  iq_stanza_struct.xml = "Malformed!!!";

  internal::PeerMessageStruct message;
  message.payload = std::move(iq_stanza_struct);

  messaging_client_->OnMessage(SignalingAddress(kFakeRemoteCorpId), message);

  ASSERT_EQ(0u, received_messages_.size());
}

TEST_F(CorpSignalStrategyTest, LocalAddressPreservedAfterDisconnect) {
  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce([](base::OnceClosure on_ready,
                   CorpMessagingClient::DoneCallback on_closed) {
        std::move(on_ready).Run();
        return testing::Return();
      });
  signal_strategy_->Connect();

  ASSERT_EQ(signal_strategy_->GetLocalAddress().id(), kFakeLocalCorpId);

  EXPECT_CALL(*messaging_client_, StopReceivingMessages());
  signal_strategy_->Disconnect();

  EXPECT_EQ(signal_strategy_->GetLocalAddress().id(), kFakeLocalCorpId);
}

TEST_F(CorpSignalStrategyTest, LocalAddressPreservedAfterChannelError) {
  CorpMessagingClient::DoneCallback on_closed_callback;
  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce([&](base::OnceClosure on_ready,
                    CorpMessagingClient::DoneCallback on_closed) {
        std::move(on_ready).Run();
        on_closed_callback = std::move(on_closed);
      });
  signal_strategy_->Connect();
  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::CONNECTED);
  ASSERT_EQ(signal_strategy_->GetLocalAddress().id(), kFakeLocalCorpId);

  EXPECT_CALL(*messaging_client_, StopReceivingMessages());
  std::move(on_closed_callback)
      .Run(HttpStatus(HttpStatus::Code::UNAVAILABLE, "error"));

  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::DISCONNECTED);
  EXPECT_EQ(signal_strategy_->GetLocalAddress().id(), kFakeLocalCorpId);
}

}  // namespace remoting
