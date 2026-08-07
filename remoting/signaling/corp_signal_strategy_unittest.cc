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
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "remoting/base/http_status.h"
#include "remoting/base/internal_headers.h"
#include "remoting/base/protobuf_http_test_responder.h"
#include "remoting/signaling/corp_messaging_client.h"
#include "remoting/signaling/jingle_message_struct_converter.h"
#include "remoting/signaling/signaling_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using testing::_;
using testing::HasSubstr;
using testing::Return;

constexpr char kFakeLocalCorpId[] = "fake_local_user@domain.com";
constexpr char kFakeRemoteCorpId[] = "fake_remote_user@domain.com";

constexpr char kFauxMessagingToken[] = "ZmF1eF9tZXNzYWdpbmdfdG9rZW4=";
constexpr char kFakeAuthzToken[] = "ZmFrZV9hdXRoel90b2tlbg==";
constexpr char kFakeToken[] = "ZmFrZV90b2tlbg==";
constexpr char kToken1[] = "dG9rZW4x";
constexpr char kToken2[] = "dG9rZW4y";



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
          received_messages_.push_back(jingle_message);
          return true;
        });
    ON_CALL(*this, OnSignalingReply(_, _))
        .WillByDefault([&](const SignalingAddress& sender_address,
                           const JingleMessageReply& jingle_reply) {
          received_replies_.push_back(jingle_reply);
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
  std::vector<JingleMessage> received_messages_;
  std::vector<JingleMessageReply> received_replies_;

 private:
  // SignalStrategy::Listener overrides.
  void OnSignalingStateChanged(SignalStrategy::State state) override {
    state_history_.push_back(state);
  }
};

TEST_F(CorpSignalStrategyTest, ConnectAndDisconnect) {
  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::DISCONNECTED);
  ASSERT_EQ(signal_strategy_->GetError(), SignalStrategy::Error::OK);
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce(base::test::RunOnceClosure<0>());
  ASSERT_EQ(state_history_.size(), 0u);
  signal_strategy_->Connect();

  ASSERT_EQ(state_history_.size(), 2u);
  ASSERT_EQ(state_history_[0], SignalStrategy::State::CONNECTING);
  ASSERT_EQ(state_history_[1], SignalStrategy::State::CONNECTED);

  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::CONNECTED);
  ASSERT_EQ(signal_strategy_->GetError(), SignalStrategy::Error::OK);
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  EXPECT_CALL(*messaging_client_, StopReceivingMessages());
  signal_strategy_->Disconnect();

  ASSERT_EQ(state_history_.size(), 3u);
  ASSERT_EQ(state_history_[2], SignalStrategy::State::DISCONNECTED);
  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::DISCONNECTED);
}

TEST_F(CorpSignalStrategyTest, StartStream_Unauthenticated) {
  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::DISCONNECTED);
  ASSERT_EQ(signal_strategy_->GetError(), SignalStrategy::Error::OK);
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce(base::test::RunOnceCallback<1>(
          HttpStatus(HttpStatus::Code::UNAUTHENTICATED, "unauthenticated")));

  signal_strategy_->Connect();

  ASSERT_EQ(state_history_.size(), 2u);
  ASSERT_EQ(state_history_[0], SignalStrategy::State::CONNECTING);
  ASSERT_EQ(state_history_[1], SignalStrategy::State::DISCONNECTED);

  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::DISCONNECTED);
  ASSERT_EQ(signal_strategy_->GetError(),
            SignalStrategy::Error::AUTHENTICATION_FAILED);
  ASSERT_TRUE(signal_strategy_->IsSignInError());
}

TEST_F(CorpSignalStrategyTest, StartStream_NetworkError) {
  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::DISCONNECTED);
  ASSERT_EQ(signal_strategy_->GetError(), SignalStrategy::Error::OK);
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce(base::test::RunOnceCallback<1>(
          HttpStatus(HttpStatus::Code::UNAVAILABLE, "unavailable")));

  signal_strategy_->Connect();

  ASSERT_EQ(state_history_.size(), 2u);
  ASSERT_EQ(state_history_[0], SignalStrategy::State::CONNECTING);
  ASSERT_EQ(state_history_[1], SignalStrategy::State::DISCONNECTED);

  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::DISCONNECTED);
  ASSERT_EQ(signal_strategy_->GetError(), SignalStrategy::Error::NETWORK_ERROR);
  ASSERT_FALSE(signal_strategy_->IsSignInError());
}


TEST_F(CorpSignalStrategyTest, SendMessage_PopulatesStructuredFields) {
  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce(base::test::RunOnceClosure<0>());
  signal_strategy_->Connect();

  // Set the token.
  {
    JingleMessage token_message;
    token_message.message_id = "id1";
    token_message.from = SignalingAddress(kFakeRemoteCorpId);
    token_message.to = SignalingAddress(kFakeLocalCorpId);
    token_message.sid = "sid123";
    token_message.SetPayload(SessionInfo());

    internal::IqStanzaStruct iq_stanza_struct =
        JingleMessageToStruct(token_message);
    iq_stanza_struct.messaging_authz_token = kFauxMessagingToken;
    internal::PeerMessageStruct message;
    message.payload = std::move(iq_stanza_struct);
    messaging_client_->OnMessage(SignalingAddress(kFakeRemoteCorpId), message);
  }

  JingleMessage jingle_message;
  jingle_message.message_id = "msg_id";
  jingle_message.from = SignalingAddress(kFakeLocalCorpId);
  jingle_message.to = SignalingAddress(kFakeRemoteCorpId);
  jingle_message.SetPayload(SessionInfo());

  EXPECT_CALL(*messaging_client_, SendMessage(_, _, _))
      .WillOnce([&](const SignalingAddress& address,
                    internal::PeerMessageStruct&& message,
                    CorpMessagingClient::DoneCallback on_done) {
        auto* iq_stanza =
            std::get_if<internal::IqStanzaStruct>(&message.payload);
        ASSERT_TRUE(iq_stanza);
        EXPECT_EQ(iq_stanza->id, "msg_id");
        EXPECT_EQ(iq_stanza->sender.local_part, "fake_local_user");
        EXPECT_EQ(iq_stanza->receiver.local_part, "fake_remote_user");
        std::move(on_done).Run(HttpStatus::OK());
      });

  signal_strategy_->SendMessage(std::move(jingle_message));
}

TEST_F(CorpSignalStrategyTest, SendMessage_NotConnected) {
  EXPECT_CALL(*messaging_client_, SendMessage(_, _, _)).Times(0);
  JingleMessage jingle_message;
  jingle_message.to = SignalingAddress(kFakeRemoteCorpId);
  jingle_message.from = SignalingAddress(kFakeLocalCorpId);
  jingle_message.SetPayload(SessionInfo());

  ASSERT_FALSE(signal_strategy_->SendMessage(std::move(jingle_message)));
}

TEST_F(CorpSignalStrategyTest, ReceiveStanza_Success) {
  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce(base::test::RunOnceClosure<0>());
  signal_strategy_->Connect();

  JingleMessage jingle_message;
  jingle_message.message_id = signal_strategy_->GetNextId();
  jingle_message.from = SignalingAddress(kFakeRemoteCorpId);
  jingle_message.to = SignalingAddress(kFakeLocalCorpId);
  jingle_message.sid = "sid123";
  jingle_message.SetPayload(SessionInfo());

  internal::IqStanzaStruct iq_stanza_struct =
      JingleMessageToStruct(jingle_message);
  iq_stanza_struct.messaging_authz_token = kFakeAuthzToken;

  internal::PeerMessageStruct message;
  message.payload = std::move(iq_stanza_struct);

  messaging_client_->OnMessage(SignalingAddress(kFakeRemoteCorpId), message);

  ASSERT_EQ(received_messages_.size(), 1u);
  EXPECT_EQ(received_messages_[0].to.id(), kFakeLocalCorpId);
  EXPECT_EQ(received_messages_[0].from.id(), kFakeRemoteCorpId);
  EXPECT_EQ(received_messages_[0].message_id, jingle_message.message_id);
  EXPECT_EQ(received_messages_[0].sid, "sid123");
}

TEST_F(CorpSignalStrategyTest, ReceiveStanza_XmlOnlyDropped) {
  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce(base::test::RunOnceClosure<0>());
  signal_strategy_->Connect();

  internal::IqStanzaStruct iq_stanza_struct;
  iq_stanza_struct.xml = "<iq>...</iq>";

  internal::PeerMessageStruct message;
  message.payload = std::move(iq_stanza_struct);

  messaging_client_->OnMessage(SignalingAddress(kFakeRemoteCorpId), message);

  ASSERT_EQ(received_messages_.size(), 0u);
}

TEST_F(CorpSignalStrategyTest, LocalAddressPreservedAfterDisconnect) {
  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce(base::test::RunOnceClosure<0>());
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

TEST_F(CorpSignalStrategyTest, ReceiveStanza_NonIqStanza) {
  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce(base::test::RunOnceClosure<0>());
  signal_strategy_->Connect();

  internal::PeerMessageStruct message;
  message.payload = internal::SystemTestStruct();

  messaging_client_->OnMessage(SignalingAddress(kFakeRemoteCorpId), message);

  ASSERT_EQ(received_messages_.size(), 0u);
}

TEST_F(CorpSignalStrategyTest, ReceiveStanza_MissingAuthzToken) {
  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce(base::test::RunOnceClosure<0>());
  signal_strategy_->Connect();

  JingleMessage jingle_message;
  jingle_message.message_id = signal_strategy_->GetNextId();
  jingle_message.from = SignalingAddress(kFakeRemoteCorpId);
  jingle_message.to = SignalingAddress(kFakeLocalCorpId);
  jingle_message.sid = "sid123";
  jingle_message.SetPayload(SessionInfo());

  internal::IqStanzaStruct iq_stanza_struct =
      JingleMessageToStruct(jingle_message);
  iq_stanza_struct.messaging_authz_token = "";

  internal::PeerMessageStruct message;
  message.payload = std::move(iq_stanza_struct);

  messaging_client_->OnMessage(SignalingAddress(kFakeRemoteCorpId), message);

  ASSERT_EQ(received_messages_.size(), 0u);
}

TEST_F(CorpSignalStrategyTest, ReceiveStanza_AuthzTokenChanged) {
  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce(base::test::RunOnceClosure<0>());
  signal_strategy_->Connect();

  JingleMessage jingle_message;
  jingle_message.from = SignalingAddress(kFakeRemoteCorpId);
  jingle_message.to = SignalingAddress(kFakeLocalCorpId);
  jingle_message.sid = "sid123";
  jingle_message.SetPayload(SessionInfo());

  // First message sets the token.
  {
    jingle_message.message_id = "id1";
    internal::IqStanzaStruct iq_stanza_struct =
        JingleMessageToStruct(jingle_message);
    iq_stanza_struct.messaging_authz_token = kToken1;
    internal::PeerMessageStruct message;
    message.payload = std::move(iq_stanza_struct);
    messaging_client_->OnMessage(SignalingAddress(kFakeRemoteCorpId), message);
  }

  // Second message changes the token.
  {
    jingle_message.message_id = "id2";
    internal::IqStanzaStruct iq_stanza_struct =
        JingleMessageToStruct(jingle_message);
    iq_stanza_struct.messaging_authz_token = kToken2;
    internal::PeerMessageStruct message;
    message.payload = std::move(iq_stanza_struct);

    EXPECT_CALL(*messaging_client_,
                SendMessage(SignalingAddress(kToken2), _, _))
        .WillOnce(base::test::RunOnceCallback<2>(HttpStatus::OK()));

    messaging_client_->OnMessage(SignalingAddress(kFakeRemoteCorpId), message);

    // Verify token change by sending a message.
    JingleMessage outbound_message;
    outbound_message.to = SignalingAddress(kFakeRemoteCorpId);
    outbound_message.from = SignalingAddress(kFakeLocalCorpId);
    outbound_message.sid = "sid123";
    outbound_message.SetPayload(SessionInfo());
    signal_strategy_->SendMessage(std::move(outbound_message));
  }
}

TEST_F(CorpSignalStrategyTest, ReceiveStanza_JingleMessageReply) {
  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce(base::test::RunOnceClosure<0>());
  signal_strategy_->Connect();

  JingleMessageReply reply;
  reply.to = SignalingAddress(kFakeLocalCorpId);
  reply.from = SignalingAddress(kFakeRemoteCorpId);
  reply.message_id = "reply_id";
  reply.reply_type = JingleMessageReply::REPLY_RESULT;

  internal::IqStanzaStruct iq_stanza_struct = JingleMessageReplyToStruct(reply);
  iq_stanza_struct.messaging_authz_token = kFakeAuthzToken;

  internal::PeerMessageStruct message;
  message.payload = std::move(iq_stanza_struct);

  messaging_client_->OnMessage(SignalingAddress(kFakeRemoteCorpId), message);

  ASSERT_EQ(received_replies_.size(), 1u);
  EXPECT_EQ(received_replies_[0].message_id, "reply_id");
}

TEST_F(CorpSignalStrategyTest, ReceiveStanza_MultipleListeners) {
  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce(base::test::RunOnceClosure<0>());
  signal_strategy_->Connect();

  class SecondaryListener : public SignalStrategy::Listener {
   public:
    MOCK_METHOD(bool,
                OnSignalingMessage,
                (const SignalingAddress&, const JingleMessage&),
                (override));
    MOCK_METHOD(bool,
                OnSignalingReply,
                (const SignalingAddress&, const JingleMessageReply&),
                (override));
    void OnSignalingStateChanged(SignalStrategy::State state) override {}
  };

  SecondaryListener secondary_listener;
  signal_strategy_->AddListener(&secondary_listener);

  // First listener returns false, second should be called.
  EXPECT_CALL(*this, OnSignalingMessage(_, _)).WillOnce(Return(false));
  EXPECT_CALL(secondary_listener, OnSignalingMessage(_, _))
      .WillOnce(Return(true));

  JingleMessage jingle_message;
  jingle_message.message_id = "id1";
  jingle_message.from = SignalingAddress(kFakeRemoteCorpId);
  jingle_message.to = SignalingAddress(kFakeLocalCorpId);
  jingle_message.sid = "sid123";
  jingle_message.SetPayload(SessionInfo());

  internal::IqStanzaStruct iq_stanza_struct =
      JingleMessageToStruct(jingle_message);
  iq_stanza_struct.messaging_authz_token = kFakeToken;
  internal::PeerMessageStruct message;
  message.payload = std::move(iq_stanza_struct);
  messaging_client_->OnMessage(SignalingAddress(kFakeRemoteCorpId), message);

  signal_strategy_->RemoveListener(&secondary_listener);
}

TEST_F(CorpSignalStrategyTest, SendMessage_MissingAuthzToken) {
  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce(base::test::RunOnceClosure<0>());
  signal_strategy_->Connect();

  // We haven't received any messages, so `messaging_authz_token_` is empty.
  EXPECT_CALL(*messaging_client_, SendMessage(_, _, _)).Times(0);

  JingleMessage jingle_message;
  jingle_message.to = SignalingAddress(kFakeRemoteCorpId);
  jingle_message.from = SignalingAddress(kFakeLocalCorpId);
  jingle_message.SetPayload(SessionInfo());
  ASSERT_FALSE(signal_strategy_->SendMessage(std::move(jingle_message)));
}

TEST_F(CorpSignalStrategyTest, SendReply_Success) {
  EXPECT_CALL(*messaging_client_, StartReceivingMessages(_, _))
      .WillOnce(base::test::RunOnceClosure<0>());
  signal_strategy_->Connect();

  // Set the token.
  JingleMessage jingle_message;
  jingle_message.message_id = "id1";
  jingle_message.from = SignalingAddress(kFakeRemoteCorpId);
  jingle_message.to = SignalingAddress(kFakeLocalCorpId);
  jingle_message.sid = "sid123";
  jingle_message.SetPayload(SessionInfo());

  internal::IqStanzaStruct iq_stanza_struct =
      JingleMessageToStruct(jingle_message);
  iq_stanza_struct.messaging_authz_token = kFakeToken;
  internal::PeerMessageStruct incoming_message;
  incoming_message.payload = std::move(iq_stanza_struct);
  messaging_client_->OnMessage(SignalingAddress(kFakeRemoteCorpId),
                               incoming_message);

  JingleMessageReply reply;
  reply.to = SignalingAddress(kFakeRemoteCorpId);
  reply.from = SignalingAddress(kFakeLocalCorpId);
  reply.message_id = "reply_id";

  EXPECT_CALL(*messaging_client_,
              SendMessage(SignalingAddress(kFakeToken), _, _))
      .WillOnce(base::test::RunOnceCallback<2>(HttpStatus::OK()));

  ASSERT_TRUE(signal_strategy_->SendReply(std::move(reply)));
}

}  // namespace remoting
