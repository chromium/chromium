// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_signal_strategy.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "remoting/base/http_status.h"
#include "remoting/base/mock_oauth_token_getter.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"
#include "remoting/signaling/jingle_message_xml_converter.h"
#include "remoting/signaling/messaging_client.h"
#include "remoting/signaling/registration_manager.h"
#include "remoting/signaling/signaling_address.h"
#include "remoting/signaling/xmpp_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

namespace {

using testing::_;
using testing::ByMove;
using testing::Mock;
using testing::Property;
using testing::Return;

constexpr char kFakeLocalUsername[] = "fake_local_user@domain.com";
constexpr char kFakeRemoteUsername[] = "fake_remote_user@domain.com";
constexpr char kFakeCorpUsername[] = "user@corp.google.com";

MATCHER_P2(SignalingMessageMatches, to, from, "") {
  const ftl::ChromotingMessage* ftl_message =
      std::get_if<ftl::ChromotingMessage>(&arg);
  if (!ftl_message || !ftl_message->has_xmpp()) {
    return false;
  }
  std::string stanza = ftl_message->xmpp().stanza();
  // Check if it's a Jingle stanza or a plain message.
  if (stanza.find("to=\"") != std::string::npos) {
    return stanza.find("to=\"" + std::string(to) + "\"") != std::string::npos &&
           stanza.find("from=\"" + std::string(from) + "\"") !=
               std::string::npos;
  }
  // For non-XMPP stanzas, just check for the payload.
  return stanza.find(std::string(to)) != std::string::npos;
}

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
      "<jingle xmlns=\"urn:xmpp:jingle:1\" action=\"session-info\" "
      "sid=\"sid123\">"
      "<rem:test-info xmlns:rem=\"google:remoting\">TestMessage</rem:test-info>"
      "</jingle>"
      "</iq>";
  auto stanza = base::WrapUnique<jingle_xmpp::XmlElement>(
      jingle_xmpp::XmlElement::ForStr(kStanzaTemplate));
  stanza->SetAttr(kQNameId, id);
  if (direction == Direction::OUTGOING) {
    stanza->SetAttr(kQNameFrom, kFakeLocalFtlId);
    stanza->SetAttr(kQNameTo, kFakeRemoteFtlId);
  } else {
    stanza->SetAttr(kQNameFrom, kFakeRemoteFtlId);
    stanza->SetAttr(kQNameTo, kFakeLocalFtlId);
  }
  return stanza;
}

class FakeMessagingClient : public MessagingClient {
 public:
  base::CallbackListSubscription RegisterMessageCallback(
      const MessageCallback& callback) override {
    return callback_list_.Add(callback);
  }

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
  }

  bool IsReceivingMessages() const override { return is_receiving_messages_; }

  MOCK_METHOD(void,
              SendMessage,
              (const SignalingAddress&, SignalingMessage&&, DoneCallback),
              (override));

  void OnMessage(const ftl::Id& sender_id,
                 const std::string& sender_registration_id,
                 const ftl::ChromotingMessage& message) {
    OnMessage(SignalingAddress::CreateFtlSignalingAddress(
                  sender_id.id(), sender_registration_id),
              message);
  }

  void OnMessage(const SignalingAddress& sender_address,
                 const ftl::ChromotingMessage& message) {
    callback_list_.Notify(sender_address, message);
  }

  void AcceptReceivingMessages() {
    std::vector<base::OnceClosure> on_started_callbacks;
    on_started_callbacks.swap(on_started_callbacks_);
    for (auto& callback : on_started_callbacks) {
      std::move(callback).Run();
    }
  }

  void RejectReceivingMessages(const HttpStatus& status) {
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
      base::RepeatingCallback<HttpStatus(std::string* out_registration_id,
                                         std::string* out_auth_token)>;

  FakeRegistrationManager() = default;
  ~FakeRegistrationManager() override = default;

  // RegistrationManager implementation.
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
      std::move(callback).Run(HttpStatus::OK());
    });
  }

  void ExpectSignInGaiaFails(const HttpStatus& status) {
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

    // By default, messages will be collected in received_messages_.
    ON_CALL(*this, OnSignalStrategyIncomingMessage(_, _))
        .WillByDefault([&](const SignalingAddress& sender_address,
                           const SignalingMessage& message) {
          if (const auto* jingle_message =
                  std::get_if<JingleMessage>(&message)) {
            received_messages_.push_back(JingleMessageToXml(*jingle_message));
            return true;
          }
          if (const auto* jingle_reply =
                  std::get_if<JingleMessageReply>(&message)) {
            received_messages_.push_back(
                JingleMessageReplyToXml(*jingle_reply));
            return true;
          }
          return false;
        });
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
          std::move(token_callback).Run(status, OAuthTokenInfo());
        });
  }

  void ExpectGetOAuthTokenSucceedsWithFakeCreds() {
    EXPECT_CALL(*token_getter_, CallWithToken(_))
        .WillOnce([](OAuthTokenGetter::TokenCallback token_callback) {
          std::move(token_callback)
              .Run(OAuthTokenGetter::SUCCESS,
                   OAuthTokenInfo(kFakeOAuthToken, kFakeLocalUsername));
        });
  }

  MOCK_METHOD(bool,
              OnSignalStrategyIncomingMessage,
              (const SignalingAddress&, const SignalingMessage&),
              (override));

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  raw_ptr<MockOAuthTokenGetter, AcrossTasksDanglingUntriaged> token_getter_ =
      nullptr;
  raw_ptr<FakeRegistrationManager, AcrossTasksDanglingUntriaged>
      registration_manager_ = nullptr;
  raw_ptr<FakeMessagingClient, AcrossTasksDanglingUntriaged> messaging_client_ =
      nullptr;
  std::unique_ptr<FtlSignalStrategy> signal_strategy_;

  std::vector<SignalStrategy::State> state_history_;
  std::vector<std::unique_ptr<jingle_xmpp::XmlElement>> received_messages_;

 private:
  // SignalStrategy::Listener overrides.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override {
    state_history_.push_back(state);
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
      HttpStatus(HttpStatus::Code::UNAUTHENTICATED, "unauthenticated"));
  EXPECT_CALL(*token_getter_, InvalidateCache()).WillOnce(Return());

  signal_strategy_->Connect();

  ASSERT_EQ(2u, state_history_.size());
  ASSERT_EQ(SignalStrategy::State::CONNECTING, state_history_[0]);
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, state_history_[1]);

  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::NETWORK_ERROR, signal_strategy_->GetError());
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
  ASSERT_TRUE(registration_manager_->IsSignedIn());
  messaging_client_->RejectReceivingMessages(
      HttpStatus(HttpStatus::Code::UNAVAILABLE, "unavailable"));
  // Remain signed-in for non-auth related error.
  ASSERT_TRUE(registration_manager_->IsSignedIn());

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
      HttpStatus(HttpStatus::Code::UNAVAILABLE, "unavailable"));
  // Remain signed-in for non-auth related error.
  ASSERT_TRUE(registration_manager_->IsSignedIn());

  ASSERT_EQ(3u, state_history_.size());
  ASSERT_EQ(SignalStrategy::State::CONNECTING, state_history_[0]);
  ASSERT_EQ(SignalStrategy::State::CONNECTED, state_history_[1]);
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, state_history_[2]);

  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::NETWORK_ERROR, signal_strategy_->GetError());
  ASSERT_FALSE(signal_strategy_->IsSignInError());
}

TEST_F(FtlSignalStrategyTest, SendMessage_XmlElement_Success) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  auto stanza =
      CreateXmlStanza(Direction::OUTGOING, signal_strategy_->GetNextId());
  std::string stanza_string = stanza->Str();

  JingleMessage jingle_message;
  std::string error;
  ASSERT_TRUE(JingleMessageFromXml(stanza.get(), &jingle_message, &error));

  EXPECT_CALL(
      *messaging_client_,
      SendMessage(Property(&SignalingAddress::id, kFakeRemoteFtlId),
                  SignalingMessageMatches(kFakeRemoteFtlId, kFakeLocalFtlId),
                  _))
      .WillOnce([&](const SignalingAddress&, SignalingMessage&&,
                    MessagingClient::DoneCallback on_done) {
        std::move(on_done).Run(HttpStatus::OK());
      });
  signal_strategy_->SendMessage(SignalingAddress(kFakeRemoteFtlId),
                                SignalingMessage(std::move(jingle_message)));
}

TEST_F(FtlSignalStrategyTest, SendMessage_XmlElement_AuthError) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  auto stanza =
      CreateXmlStanza(Direction::OUTGOING, signal_strategy_->GetNextId());

  JingleMessage jingle_message;
  std::string error;
  ASSERT_TRUE(JingleMessageFromXml(stanza.get(), &jingle_message, &error));

  EXPECT_CALL(*token_getter_, InvalidateCache()).WillOnce(Return());
  EXPECT_CALL(
      *messaging_client_,
      SendMessage(Property(&SignalingAddress::id, kFakeRemoteFtlId), _, _))
      .WillOnce([](const SignalingAddress&, SignalingMessage&&,
                   MessagingClient::DoneCallback on_done) {
        std::move(on_done).Run(
            HttpStatus(HttpStatus::Code::UNAUTHENTICATED, "unauthenticated"));
      });
  signal_strategy_->SendMessage(SignalingAddress(kFakeRemoteFtlId),
                                SignalingMessage(std::move(jingle_message)));

  ASSERT_EQ(3u, state_history_.size());
  ASSERT_EQ(SignalStrategy::State::CONNECTING, state_history_[0]);
  ASSERT_EQ(SignalStrategy::State::CONNECTED, state_history_[1]);
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, state_history_[2]);

  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::NETWORK_ERROR, signal_strategy_->GetError());
  ASSERT_FALSE(signal_strategy_->IsSignInError());
}

TEST_F(FtlSignalStrategyTest, SendMessage_XmlElement_NetworkError) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  std::string stanza_id = signal_strategy_->GetNextId();
  auto stanza = CreateXmlStanza(Direction::OUTGOING, stanza_id);

  JingleMessage jingle_message;
  std::string error;
  ASSERT_TRUE(JingleMessageFromXml(stanza.get(), &jingle_message, &error));

  EXPECT_CALL(
      *messaging_client_,
      SendMessage(Property(&SignalingAddress::id, kFakeRemoteFtlId), _, _))
      .WillOnce([&](const SignalingAddress&, SignalingMessage&&,
                    MessagingClient::DoneCallback on_done) {
        std::move(on_done).Run(
            HttpStatus(HttpStatus::Code::UNAVAILABLE, "unavailable"));
      });
  signal_strategy_->SendMessage(SignalingAddress(kFakeRemoteFtlId),
                                SignalingMessage(std::move(jingle_message)));

  ASSERT_EQ(1u, received_messages_.size());
  auto& error_message = received_messages_[0];
  ASSERT_EQ(kIqTypeError, error_message->Attr(kQNameType));
  ASSERT_EQ(stanza_id, error_message->Attr(kQNameId));
  ASSERT_EQ(kFakeRemoteFtlId, error_message->Attr(kQNameFrom));
  ASSERT_EQ(kFakeLocalFtlId, error_message->Attr(kQNameTo));
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
  // The attribute order may change during XML conversion.
  std::string received_stanza_string = received_messages_[0]->Str();
  EXPECT_THAT(
      received_stanza_string,
      testing::HasSubstr("to=\"" + std::string(kFakeLocalFtlId) + "\""));
  EXPECT_THAT(
      received_stanza_string,
      testing::HasSubstr("from=\"" + std::string(kFakeRemoteFtlId) + "\""));
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

  EXPECT_CALL(*this, OnSignalStrategyIncomingMessage(_, _))
      .WillOnce([&](const SignalingAddress& sender_address,
                    const SignalingMessage& received_message) {
        SignalingAddress expected_address =
            SignalingAddress::CreateFtlSignalingAddress(
                kFakeRemoteUsername, kFakeRemoteRegistrationId);
        EXPECT_EQ(expected_address.id(), sender_address.id());

        const auto* jingle_message =
            std::get_if<JingleMessage>(&received_message);
        EXPECT_TRUE(jingle_message);
        if (jingle_message) {
          std::string received_stanza_string =
              JingleMessageToXml(*jingle_message)->Str();
          EXPECT_THAT(received_stanza_string,
                      testing::HasSubstr("to=\"" +
                                         std::string(kFakeLocalFtlId) + "\""));
          EXPECT_THAT(received_stanza_string,
                      testing::HasSubstr("from=\"" +
                                         std::string(kFakeRemoteFtlId) + "\""));
        }
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

TEST_F(FtlSignalStrategyTest, SendMessage_Success) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  std::string message_payload("Woah dude!  It's a Chromoting message!!");
  ftl::ChromotingMessage message;
  message.mutable_xmpp()->set_stanza(message_payload);

  EXPECT_CALL(
      *messaging_client_,
      SendMessage(Property(&SignalingAddress::id,
                           SignalingAddress::CreateFtlSignalingAddress(
                               kFakeRemoteUsername, kFakeRemoteRegistrationId)
                               .id()),
                  SignalingMessageMatches(message_payload, ""), _))
      .WillOnce([](const SignalingAddress&, SignalingMessage&&,
                   MessagingClient::DoneCallback on_done) {
        std::move(on_done).Run(HttpStatus::OK());
      });

  signal_strategy_->SendMessage(
      SignalingAddress::CreateFtlSignalingAddress(kFakeRemoteUsername,
                                                  kFakeRemoteRegistrationId),
      SignalingMessage{message});
}

TEST_F(FtlSignalStrategyTest, SendMessage_AuthError) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  EXPECT_CALL(*token_getter_, InvalidateCache()).WillOnce(Return());
  EXPECT_CALL(
      *messaging_client_,
      SendMessage(Property(&SignalingAddress::id,
                           SignalingAddress::CreateFtlSignalingAddress(
                               kFakeRemoteUsername, kFakeRemoteRegistrationId)
                               .id()),
                  _, _))
      .WillOnce([](const SignalingAddress&, SignalingMessage&&,
                   MessagingClient::DoneCallback on_done) {
        std::move(on_done).Run(
            HttpStatus(HttpStatus::Code::UNAUTHENTICATED, "unauthenticated"));
      });

  ftl::ChromotingMessage message;
  signal_strategy_->SendMessage(
      SignalingAddress::CreateFtlSignalingAddress(kFakeRemoteUsername,
                                                  kFakeRemoteRegistrationId),
      SignalingMessage{message});

  ASSERT_EQ(3u, state_history_.size());
  ASSERT_EQ(SignalStrategy::State::CONNECTING, state_history_[0]);
  ASSERT_EQ(SignalStrategy::State::CONNECTED, state_history_[1]);
  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, state_history_[2]);

  ASSERT_EQ(SignalStrategy::State::DISCONNECTED, signal_strategy_->GetState());
  ASSERT_EQ(SignalStrategy::Error::NETWORK_ERROR, signal_strategy_->GetError());
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  // Sign-out due to auth related error.
  ASSERT_FALSE(registration_manager_->IsSignedIn());
}

TEST_F(FtlSignalStrategyTest, SendMessage_NetworkError) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  EXPECT_CALL(
      *messaging_client_,
      SendMessage(Property(&SignalingAddress::id,
                           SignalingAddress::CreateFtlSignalingAddress(
                               kFakeRemoteUsername, kFakeRemoteRegistrationId)
                               .id()),
                  _, _))
      .WillOnce([](const SignalingAddress&, SignalingMessage&&,
                   MessagingClient::DoneCallback on_done) {
        std::move(on_done).Run(
            HttpStatus(HttpStatus::Code::UNAVAILABLE, "unavailable"));
      });

  ftl::ChromotingMessage message;
  signal_strategy_->SendMessage(
      SignalingAddress::CreateFtlSignalingAddress(kFakeRemoteUsername,
                                                  kFakeRemoteRegistrationId),
      SignalingMessage{message});

  ASSERT_EQ(0u, received_messages_.size());
  // Remain signed-in for non-auth related error.
  ASSERT_TRUE(registration_manager_->IsSignedIn());
}

TEST_F(FtlSignalStrategyTest, ReceiveMessageFromNonFtlSender_IsIgnored) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  auto stanza =
      CreateXmlStanza(Direction::INCOMING, signal_strategy_->GetNextId());
  std::string stanza_string = stanza->Str();
  ftl::ChromotingMessage message;
  message.mutable_xmpp()->set_stanza(stanza_string);

  // This represents an XMPP address.
  messaging_client_->OnMessage(SignalingAddress(kFakeRemoteUsername), message);
  ASSERT_EQ(received_messages_.size(), 0u);

  // This represents a 'Corp' signaling user with an FTL-like resource. The
  // SignalingAddress class should detect this and mark it as a Corp address.
  messaging_client_->OnMessage(
      SignalingAddress::CreateFtlSignalingAddress(kFakeCorpUsername,
                                                  kFakeRemoteRegistrationId),
      message);
  ASSERT_EQ(received_messages_.size(), 0u);
}

}  // namespace remoting
