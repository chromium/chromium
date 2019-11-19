// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_signal_strategy.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"
#include "remoting/signaling/messaging_client.h"
#include "remoting/signaling/registration_manager.h"
#include "remoting/signaling/signaling_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/grpc/src/include/grpc/status.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/libjingle_xmpp/xmpp/constants.h"

namespace remoting {

namespace {

using testing::_;
using testing::ByMove;
using testing::Mock;
using testing::Return;

constexpr char kFakeLocalUsername[] = "fake_local_user@domain.com";
constexpr char kFakeRemoteUsername[] = "fake_remote_user@domain.com";
constexpr char kFakeOAuthToken[] = "fake_oauth_token";
constexpr char kFakeFtlAuthToken[] = "fake_auth_token";
constexpr char kFakeLocalRegistrationId[] = "fake_local_registration_id";
constexpr char kFakeRemoteRegistrationId[] = "fake_remote_registration_id";
constexpr char kFakeLocalFtlId[] =
    "fake_local_user@domain.com/chromoting_ftl_fake_local_registration_id";
constexpr char kFakeRemoteFtlId[] =
    "fake_remote_user@domain.com/chromoting_ftl_fake_remote_registration_id";

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
  stanza->SetAttr(jingle_xmpp::QN_ID, id);
  if (direction == Direction::OUTGOING) {
    stanza->SetAttr(jingle_xmpp::QN_FROM, kFakeLocalFtlId);
    stanza->SetAttr(jingle_xmpp::QN_TO, kFakeRemoteFtlId);
  } else {
    stanza->SetAttr(jingle_xmpp::QN_FROM, kFakeRemoteFtlId);
    stanza->SetAttr(jingle_xmpp::QN_TO, kFakeLocalFtlId);
  }
  return stanza;
}

class MockOAuthTokenGetter : public OAuthTokenGetter {
 public:
  MOCK_METHOD1(CallWithToken, void(TokenCallback));
  MOCK_METHOD0(InvalidateCache, void());
};

class FakeMessagingClient : public MessagingClient {
 public:
  std::unique_ptr<MessageCallbackSubscription> RegisterMessageCallback(
      const MessageCallback& callback) override {
    return callback_list_.Add(callback);
  }

  void PullMessages(DoneCallback on_done) override { NOTREACHED(); }

  void StartReceivingMessages(base::OnceClosure on_started,
                              DoneCallback on_closed) override {
    if (is_receiving_messages_) {
      std::move(on_started).Run();
      return;
    }
    on_started_callbacks_.push_back(std::move(on_started));
    on_closed_callbacks_.push_back(std::move(on_closed));
    is_receiving_messages_ = true;
  }

  void StopReceivingMessages() override {
    if (!is_receiving_messages_) {
      return;
    }
    RejectReceivingMessages(grpc::Status::CANCELLED);
  }

  bool IsReceivingMessages() const override { return is_receiving_messages_; }

  MOCK_METHOD4(SendMessage,
               void(const std::string&,
                    const std::string&,
                    const ftl::ChromotingMessage&,
                    DoneCallback));

  void OnMessage(const ftl::Id& sender_id,
                 const std::string& sender_registration_id,
                 const ftl::ChromotingMessage& message) {
    callback_list_.Notify(sender_id, sender_registration_id, message);
  }

  void AcceptReceivingMessages() {
    std::vector<base::OnceClosure> on_started_callbacks;
    on_started_callbacks.swap(on_started_callbacks_);
    for (auto& callback : on_started_callbacks) {
      std::move(callback).Run();
    }
  }

  void RejectReceivingMessages(const grpc::Status& status) {
    DCHECK(is_receiving_messages_);
    std::vector<DoneCallback> on_closed_callbacks;
    on_closed_callbacks.swap(on_closed_callbacks_);
    for (auto& callback : on_closed_callbacks) {
      std::move(callback).Run(status);
    }
    is_receiving_messages_ = false;
  }

 private:
  MessageCallbackList callback_list_;
  bool is_receiving_messages_ = false;
  std::vector<base::OnceClosure> on_started_callbacks_;
  std::vector<DoneCallback> on_closed_callbacks_;
};

class FakeRegistrationManager : public RegistrationManager {
 public:
  using SignInCallback =
      base::RepeatingCallback<grpc::Status(std::string* out_registration_id,
                                           std::string* out_auth_token)>;

  FakeRegistrationManager() = default;
  ~FakeRegistrationManager() override = default;

  // RegistrationManager implementations.
  void SignOut() override { is_signed_in_ = false; }

  bool IsSignedIn() const override { return is_signed_in_; }

  std::string GetRegistrationId() const override {
    return is_signed_in_ ? kFakeLocalRegistrationId : "";
  }

  std::string GetFtlAuthToken() const override {
    return is_signed_in_ ? kFakeFtlAuthToken : "";
  }

  MOCK_METHOD1(SignInGaia, void(DoneCallback));

  void ExpectSignInGaiaSucceeds() {
    EXPECT_CALL(*this, SignInGaia(_)).WillOnce([&](DoneCallback callback) {
      is_signed_in_ = true;
      std::move(callback).Run(grpc::Status::OK);
    });
  }

  void ExpectSignInGaiaFails(const grpc::Status& status) {
    EXPECT_CALL(*this, SignInGaia(_)).WillOnce([status](DoneCallback callback) {
      std::move(callback).Run(status);
    });
  }

 private:
  bool is_signed_in_ = false;
};

}  // namespace

class FtlSignalStrategyTest : public testing::Test,
                              public SignalStrategy::Listener {
 public:
  FtlSignalStrategyTest() {
    auto token_getter = std::make_unique<MockOAuthTokenGetter>();
    auto registration_manager = std::make_unique<FakeRegistrationManager>();
    auto messaging_client = std::make_unique<FakeMessagingClient>();

    token_getter_ = token_getter.get();
    registration_manager_ = registration_manager.get();
    messaging_client_ = messaging_client.get();

    signal_strategy_.reset(new FtlSignalStrategy(
        std::move(token_getter), std::move(registration_manager),
        std::move(messaging_client)));
    signal_strategy_->AddListener(this);

    // By default, messages will be delievered through
    // OnSignalStrategyIncomingStanza().
    ON_CALL(*this, OnSignalStrategyIncomingMessage(_, _, _))
        .WillByDefault(Return(false));
  }

  ~FtlSignalStrategyTest() override {
    signal_strategy_->RemoveListener(this);
    signal_strategy_.reset();
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  void ExpectGetOAuthTokenFails(OAuthTokenGetter::Status status) {
    EXPECT_CALL(*token_getter_, CallWithToken(_))
        .WillOnce([=](OAuthTokenGetter::TokenCallback token_callback) {
          std::move(token_callback).Run(status, {}, {});
        });
  }

  void ExpectGetOAuthTokenSucceedsWithFakeCreds() {
    EXPECT_CALL(*token_getter_, CallWithToken(_))
        .WillOnce([](OAuthTokenGetter::TokenCallback token_callback) {
          std::move(token_callback)
              .Run(OAuthTokenGetter::SUCCESS, kFakeLocalUsername,
                   kFakeOAuthToken);
        });
  }

  MOCK_METHOD3(OnSignalStrategyIncomingMessage,
               bool(const ftl::Id&,
                    const std::string&,
                    const ftl::ChromotingMessage&));

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  MockOAuthTokenGetter* token_getter_ = nullptr;
  FakeRegistrationManager* registration_manager_ = nullptr;
  FakeMessagingClient* messaging_client_ = nullptr;
  std::unique_ptr<FtlSignalStrategy> signal_strategy_;

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

TEST_F(FtlSignalStrategyTest, OAuthTokenGetterAuthError) {
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::OK, signal_strategy_->GetError());
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  ExpectGetOAuthTokenFails(OAuthTokenGetter::AUTH_ERROR);

  signal_strategy_->Connect();

  ASSERT_EQ(2u, state_history_.size());
  ASSERT_EQ(SignalStrategy::State::CONNECTING, state_history_[0]);
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, state_history_[1]);

  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::AUTHENTICATION_FAILED,
            signal_strategy_->GetError());
  ASSERT_TRUE(signal_strategy_->IsSignInError());
}

TEST_F(FtlSignalStrategyTest, SignInGaiaAuthError_InvalidatesOAuthToken) {
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::OK, signal_strategy_->GetError());
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaFails(
      grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "unauthenticated"));
  EXPECT_CALL(*token_getter_, InvalidateCache()).WillOnce(Return());

  signal_strategy_->Connect();

  ASSERT_EQ(2u, state_history_.size());
  ASSERT_EQ(SignalStrategy::State::CONNECTING, state_history_[0]);
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, state_history_[1]);

  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::AUTHENTICATION_FAILED,
            signal_strategy_->GetError());
  ASSERT_TRUE(signal_strategy_->IsSignInError());
}

TEST_F(FtlSignalStrategyTest, StartStream_Success) {
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::OK, signal_strategy_->GetError());
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();

  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  ASSERT_EQ(2u, state_history_.size());
  ASSERT_EQ(SignalStrategy::State::CONNECTING, state_history_[0]);
  ASSERT_EQ(SignalStrategy::State::CONNECTED, state_history_[1]);

  ASSERT_EQ(SignalStrategy::State::CONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::OK, signal_strategy_->GetError());
  ASSERT_FALSE(signal_strategy_->IsSignInError());
}

TEST_F(FtlSignalStrategyTest, StartStream_Failure) {
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::OK, signal_strategy_->GetError());
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();

  signal_strategy_->Connect();
  messaging_client_->RejectReceivingMessages(
      grpc::Status(grpc::StatusCode::UNAVAILABLE, "unavailable"));

  ASSERT_EQ(2u, state_history_.size());
  ASSERT_EQ(SignalStrategy::State::CONNECTING, state_history_[0]);
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, state_history_[1]);

  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::NETWORK_ERROR, signal_strategy_->GetError());
  ASSERT_FALSE(signal_strategy_->IsSignInError());
}

TEST_F(FtlSignalStrategyTest, StreamRemotelyClosed) {
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::OK, signal_strategy_->GetError());
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();

  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();
  messaging_client_->RejectReceivingMessages(
      grpc::Status(grpc::StatusCode::UNAVAILABLE, "unavailable"));

  ASSERT_EQ(3u, state_history_.size());
  ASSERT_EQ(SignalStrategy::State::CONNECTING, state_history_[0]);
  ASSERT_EQ(SignalStrategy::State::CONNECTED, state_history_[1]);
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, state_history_[2]);

  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::NETWORK_ERROR, signal_strategy_->GetError());
  ASSERT_FALSE(signal_strategy_->IsSignInError());
}

TEST_F(FtlSignalStrategyTest, SendStanza_Success) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  auto stanza =
      CreateXmlStanza(Direction::OUTGOING, signal_strategy_->GetNextId());
  std::string stanza_string = stanza->Str();

  EXPECT_CALL(*messaging_client_,
              SendMessage(kFakeRemoteUsername, kFakeRemoteRegistrationId, _, _))
      .WillOnce([stanza_string](const std::string&, const std::string&,
                                const ftl::ChromotingMessage& message,
                                MessagingClient::DoneCallback on_done) {
        ASSERT_EQ(stanza_string, message.xmpp().stanza());
        std::move(on_done).Run(grpc::Status::OK);
      });
  signal_strategy_->SendStanza(std::move(stanza));
}

TEST_F(FtlSignalStrategyTest, SendStanza_AuthError) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  auto stanza =
      CreateXmlStanza(Direction::OUTGOING, signal_strategy_->GetNextId());

  EXPECT_CALL(*token_getter_, InvalidateCache()).WillOnce(Return());
  EXPECT_CALL(*messaging_client_,
              SendMessage(kFakeRemoteUsername, kFakeRemoteRegistrationId, _, _))
      .WillOnce([](const std::string&, const std::string&,
                   const ftl::ChromotingMessage& message,
                   MessagingClient::DoneCallback on_done) {
        std::move(on_done).Run(
            grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "unauthenticated"));
      });
  signal_strategy_->SendStanza(std::move(stanza));

  ASSERT_EQ(3u, state_history_.size());
  ASSERT_EQ(SignalStrategy::State::CONNECTING, state_history_[0]);
  ASSERT_EQ(SignalStrategy::State::CONNECTED, state_history_[1]);
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, state_history_[2]);

  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::AUTHENTICATION_FAILED,
            signal_strategy_->GetError());
  ASSERT_FALSE(signal_strategy_->IsSignInError());
}

TEST_F(FtlSignalStrategyTest, SendStanza_NetworkError) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  std::string stanza_id = signal_strategy_->GetNextId();
  auto stanza = CreateXmlStanza(Direction::OUTGOING, stanza_id);

  EXPECT_CALL(*messaging_client_,
              SendMessage(kFakeRemoteUsername, kFakeRemoteRegistrationId, _, _))
      .WillOnce([](const std::string&, const std::string&,
                   const ftl::ChromotingMessage& message,
                   MessagingClient::DoneCallback on_done) {
        std::move(on_done).Run(
            grpc::Status(grpc::StatusCode::UNAVAILABLE, "unavailable"));
      });
  signal_strategy_->SendStanza(std::move(stanza));

  ASSERT_EQ(1u, received_messages_.size());
  auto& error_message = received_messages_[0];
  ASSERT_EQ(jingle_xmpp::STR_ERROR, error_message->Attr(jingle_xmpp::QN_TYPE));
  ASSERT_EQ(stanza_id, error_message->Attr(jingle_xmpp::QN_ID));
  ASSERT_EQ(kFakeRemoteFtlId, error_message->Attr(jingle_xmpp::QN_FROM));
  ASSERT_EQ(kFakeLocalFtlId, error_message->Attr(jingle_xmpp::QN_TO));
}

TEST_F(FtlSignalStrategyTest, ReceiveStanza_Success) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  auto stanza =
      CreateXmlStanza(Direction::INCOMING, signal_strategy_->GetNextId());
  std::string stanza_string = stanza->Str();
  ftl::ChromotingMessage message;
  message.mutable_xmpp()->set_stanza(stanza_string);
  ftl::Id remote_user_id;
  remote_user_id.set_type(ftl::IdType_Type_EMAIL);
  remote_user_id.set_id(kFakeRemoteUsername);
  messaging_client_->OnMessage(remote_user_id, kFakeRemoteRegistrationId,
                               message);

  ASSERT_EQ(1u, received_messages_.size());
  ASSERT_EQ(stanza_string, received_messages_[0]->Str());
}

TEST_F(FtlSignalStrategyTest, ReceiveMessage_DelieverMessageAndDropStanza) {
  ftl::Id remote_user_id;
  remote_user_id.set_type(ftl::IdType_Type_EMAIL);
  remote_user_id.set_id(kFakeRemoteUsername);

  auto stanza =
      CreateXmlStanza(Direction::INCOMING, signal_strategy_->GetNextId());
  std::string stanza_string = stanza->Str();
  ftl::ChromotingMessage message;
  message.mutable_xmpp()->set_stanza(stanza_string);

  EXPECT_CALL(*this,
              OnSignalStrategyIncomingMessage(_, kFakeRemoteRegistrationId, _))
      .WillOnce([&](const ftl::Id& sender_id,
                    const std::string& sender_registration_id_unused,
                    const ftl::ChromotingMessage& message) {
        EXPECT_EQ(ftl::IdType_Type_EMAIL, sender_id.type());
        EXPECT_EQ(remote_user_id.id(), sender_id.id());
        EXPECT_EQ(stanza_string, message.xmpp().stanza());
        return true;
      });

  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  messaging_client_->OnMessage(remote_user_id, kFakeRemoteRegistrationId,
                               message);

  // Message has already been consumed in OnSignalStrategyIncomingMessage().
  ASSERT_EQ(0u, received_messages_.size());
}

TEST_F(FtlSignalStrategyTest, ReceiveStanza_DropMessageWithMalformedXmpp) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  ftl::ChromotingMessage message;
  message.mutable_xmpp()->set_stanza("Malformed!!!");
  ftl::Id remote_user_id;
  remote_user_id.set_type(ftl::IdType_Type_EMAIL);
  remote_user_id.set_id(kFakeRemoteUsername);
  messaging_client_->OnMessage(remote_user_id, kFakeRemoteRegistrationId,
                               message);

  ASSERT_EQ(0u, received_messages_.size());
}

}  // namespace remoting
