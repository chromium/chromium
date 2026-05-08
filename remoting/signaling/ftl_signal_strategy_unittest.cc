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
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "remoting/base/http_status.h"
#include "remoting/base/mock_oauth_token_getter.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/proto/ftl/v1/chromoting_message.pb.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"
#include "remoting/proto/ftl/v1/xmpp.pb.h"
#include "remoting/signaling/ftl_messaging_client.h"
#include "remoting/signaling/jingle_message_xml_converter.h"
#include "remoting/signaling/registration_manager.h"
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
using testing::Property;
using testing::Return;

constexpr char kFakeLocalUsername[] = "fake_local_user@domain.com";
constexpr char kFakeRemoteUsername[] = "fake_remote_user@domain.com";
constexpr char kFakeCorpUsername[] = "user@corp.google.com";

MATCHER_P2(SignalingMessageMatches, to, from, "") {
  if (!arg.has_xmpp()) {
    return false;
  }

  // Check XML stanza
  bool xml_matches = false;
  if (arg.xmpp().has_stanza()) {
    std::string stanza = arg.xmpp().stanza();
    auto parsed_xml = base::WrapUnique(jingle_xmpp::XmlElement::ForStr(stanza));
    if (parsed_xml && parsed_xml->Attr(kQNameTo) == std::string(to) &&
        parsed_xml->Attr(kQNameFrom) == std::string(from)) {
      xml_matches = true;
    }
  }

  // Check iq_stanza
  bool iq_matches = false;
  if (arg.xmpp().has_iq_stanza()) {
    const auto& iq_stanza = arg.xmpp().iq_stanza();
    auto get_id = [](const ftl::JabberId& jid) {
      std::string id = jid.local_part();
      if (!jid.resource_part().empty()) {
        id += "/chromoting_ftl_" + jid.resource_part();
      }
      return id;
    };
    if (get_id(iq_stanza.sender()) == std::string(from) &&
        get_id(iq_stanza.receiver()) == std::string(to)) {
      iq_matches = true;
    }
  }

  return xml_matches && iq_matches;
}

MATCHER_P(SignalingMessageMatches, to, "") {
  return arg.has_xmpp() &&
         arg.xmpp().stanza().find(std::string(to)) != std::string::npos;
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

ftl::ChromotingMessage CreateIqStanzaMessage(Direction direction,
                                             const std::string& id) {
  ftl::ChromotingMessage message;
  auto* xmpp = message.mutable_xmpp();
  auto* iq = xmpp->mutable_iq_stanza();
  iq->set_id(id);
  auto* jingle = iq->mutable_jingle();
  jingle->set_session_id("sid123");
  jingle->mutable_session_info();

  if (direction == Direction::OUTGOING) {
    iq->mutable_sender()->set_local_part(kFakeLocalUsername);
    iq->mutable_sender()->set_resource_part(kFakeLocalRegistrationId);
    iq->mutable_receiver()->set_local_part(kFakeRemoteUsername);
    iq->mutable_receiver()->set_resource_part(kFakeRemoteRegistrationId);
  } else {
    iq->mutable_sender()->set_local_part(kFakeRemoteUsername);
    iq->mutable_sender()->set_resource_part(kFakeRemoteRegistrationId);
    iq->mutable_receiver()->set_local_part(kFakeLocalUsername);
    iq->mutable_receiver()->set_resource_part(kFakeLocalRegistrationId);
  }
  return message;
}

class FakeMessagingClient : public FtlMessagingClient {
 public:
  FakeMessagingClient()
      // static_cast is used to disambiguate which c'tor to call.
      : FtlMessagingClient(static_cast<OAuthTokenGetter*>(nullptr),
                           nullptr,
                           static_cast<RegistrationManager*>(nullptr),
                           nullptr) {}
  ~FakeMessagingClient() override = default;

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

  void StopReceivingMessages() override { is_receiving_messages_ = false; }

  bool IsReceivingMessages() const override { return is_receiving_messages_; }

  MOCK_METHOD(void,
              SendMessage,
              (const SignalingAddress&,
               ftl::ChromotingMessage&&,
               DoneCallback,
               scoped_refptr<const ProtobufHttpRequestConfig::RetryPolicy>),
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

  MOCK_METHOD(void, SignInGaia, (DoneCallback), (override));

  void ExpectSignInGaiaSucceeds() {
    EXPECT_CALL(*this, SignInGaia(_)).WillOnce([&](DoneCallback callback) {
      is_signed_in_ = true;
      std::move(callback).Run(HttpStatus::OK());
    });
  }

  void ExpectSignInGaiaFails(const HttpStatus& status) {
    EXPECT_CALL(*this, SignInGaia(_))
        .WillOnce(base::test::RunOnceCallback<0>(status));
  }

 private:
  bool is_signed_in_ = false;
};

}  // namespace

class FtlSignalStrategyTest : public testing::Test,
                              public SignalStrategy::Listener,
                              public FtlSignalStrategy::FtlListener {
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
    signal_strategy_->AddFtlListener(this);

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

  ~FtlSignalStrategyTest() override {
    signal_strategy_->RemoveListener(this);
    signal_strategy_->RemoveFtlListener(this);
    signal_strategy_.reset();
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  void ExpectGetOAuthTokenFails(OAuthTokenGetter::Status status) {
    EXPECT_CALL(*token_getter_, CallWithToken(_))
        .WillOnce(base::test::RunOnceCallback<0>(status, OAuthTokenInfo()));
  }

  void ExpectGetOAuthTokenSucceedsWithFakeCreds() {
    EXPECT_CALL(*token_getter_, CallWithToken(_))
        .WillOnce(base::test::RunOnceCallback<0>(
            OAuthTokenGetter::SUCCESS,
            OAuthTokenInfo(kFakeOAuthToken, kFakeLocalUsername)));
  }

  MOCK_METHOD(bool,
              OnSignalingMessage,
              (const SignalingAddress&, const JingleMessage&),
              (override));

  MOCK_METHOD(bool,
              OnSignalingReply,
              (const SignalingAddress&, const JingleMessageReply&),
              (override));

  MOCK_METHOD(bool,
              OnIncomingFtlMessage,
              (const SignalingAddress&, const ftl::ChromotingMessage&),
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
  void OnSignalingStateChanged(SignalStrategy::State state) override {
    state_history_.push_back(state);
  }
};

TEST_F(FtlSignalStrategyTest, OAuthTokenGetterAuthError) {
  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::DISCONNECTED);
  ASSERT_EQ(signal_strategy_->GetError(), SignalStrategy::Error::OK);
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  ExpectGetOAuthTokenFails(OAuthTokenGetter::AUTH_ERROR);

  signal_strategy_->Connect();

  ASSERT_EQ(state_history_.size(), 2u);
  ASSERT_EQ(state_history_[0], SignalStrategy::State::CONNECTING);
  ASSERT_EQ(state_history_[1], SignalStrategy::State::DISCONNECTED);

  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::DISCONNECTED);
  ASSERT_EQ(signal_strategy_->GetError(),
            SignalStrategy::Error::AUTHENTICATION_FAILED);
  ASSERT_TRUE(signal_strategy_->IsSignInError());
}

TEST_F(FtlSignalStrategyTest, SignInGaiaAuthError_InvalidatesOAuthToken) {
  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::DISCONNECTED);
  ASSERT_EQ(signal_strategy_->GetError(), SignalStrategy::Error::OK);
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaFails(
      HttpStatus(HttpStatus::Code::UNAUTHENTICATED, "unauthenticated"));
  EXPECT_CALL(*token_getter_, InvalidateCache()).WillOnce(Return());

  signal_strategy_->Connect();

  ASSERT_EQ(state_history_.size(), 2u);
  ASSERT_EQ(state_history_[0], SignalStrategy::State::CONNECTING);
  ASSERT_EQ(state_history_[1], SignalStrategy::State::DISCONNECTED);

  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::DISCONNECTED);
  ASSERT_EQ(signal_strategy_->GetError(), SignalStrategy::Error::NETWORK_ERROR);
  ASSERT_TRUE(signal_strategy_->IsSignInError());
}

TEST_F(FtlSignalStrategyTest, StartStream_Success) {
  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::DISCONNECTED);
  ASSERT_EQ(signal_strategy_->GetError(), SignalStrategy::Error::OK);
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();

  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  ASSERT_EQ(state_history_.size(), 2u);
  ASSERT_EQ(state_history_[0], SignalStrategy::State::CONNECTING);
  ASSERT_EQ(state_history_[1], SignalStrategy::State::CONNECTED);

  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::CONNECTED);
  ASSERT_EQ(signal_strategy_->GetError(), SignalStrategy::Error::OK);
  ASSERT_FALSE(signal_strategy_->IsSignInError());
}

TEST_F(FtlSignalStrategyTest, StartStream_Failure) {
  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::DISCONNECTED);
  ASSERT_EQ(signal_strategy_->GetError(), SignalStrategy::Error::OK);
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();

  signal_strategy_->Connect();
  ASSERT_TRUE(registration_manager_->IsSignedIn());
  messaging_client_->RejectReceivingMessages(
      HttpStatus(HttpStatus::Code::UNAVAILABLE, "unavailable"));
  // Remain signed-in for non-auth related error.
  ASSERT_TRUE(registration_manager_->IsSignedIn());

  ASSERT_EQ(state_history_.size(), 2u);
  ASSERT_EQ(state_history_[0], SignalStrategy::State::CONNECTING);
  ASSERT_EQ(state_history_[1], SignalStrategy::State::DISCONNECTED);

  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::DISCONNECTED);
  ASSERT_EQ(signal_strategy_->GetError(), SignalStrategy::Error::NETWORK_ERROR);
  ASSERT_FALSE(signal_strategy_->IsSignInError());
}

TEST_F(FtlSignalStrategyTest, StreamRemotelyClosed) {
  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::DISCONNECTED);
  ASSERT_EQ(signal_strategy_->GetError(), SignalStrategy::Error::OK);
  ASSERT_FALSE(signal_strategy_->IsSignInError());

  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();

  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();
  messaging_client_->RejectReceivingMessages(
      HttpStatus(HttpStatus::Code::UNAVAILABLE, "unavailable"));
  // Remain signed-in for non-auth related error.
  ASSERT_TRUE(registration_manager_->IsSignedIn());

  ASSERT_EQ(state_history_.size(), 3u);
  ASSERT_EQ(state_history_[0], SignalStrategy::State::CONNECTING);
  ASSERT_EQ(state_history_[1], SignalStrategy::State::CONNECTED);
  ASSERT_EQ(state_history_[2], SignalStrategy::State::DISCONNECTED);

  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::DISCONNECTED);
  ASSERT_EQ(signal_strategy_->GetError(), SignalStrategy::Error::NETWORK_ERROR);
  ASSERT_FALSE(signal_strategy_->IsSignInError());
}

// TODO: crbug.com/504910955 - Re-enable after iq_stanza is fixed.
TEST_F(FtlSignalStrategyTest, DISABLED_SendMessage_XmlElement_Success) {
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
                  SignalingMessageMatches(kFakeRemoteFtlId, kFakeLocalFtlId), _,
                  _))
      .WillOnce(base::test::RunOnceCallback<2>(HttpStatus::OK()));
  signal_strategy_->SendMessage(std::move(jingle_message));
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
      SendMessage(Property(&SignalingAddress::id, kFakeRemoteFtlId), _, _, _))
      .WillOnce(base::test::RunOnceCallback<2>(
          HttpStatus(HttpStatus::Code::UNAUTHENTICATED, "unauthenticated")));
  signal_strategy_->SendMessage(std::move(jingle_message));

  ASSERT_EQ(state_history_.size(), 3u);
  ASSERT_EQ(state_history_[0], SignalStrategy::State::CONNECTING);
  ASSERT_EQ(state_history_[1], SignalStrategy::State::CONNECTED);
  ASSERT_EQ(state_history_[2], SignalStrategy::State::DISCONNECTED);

  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::DISCONNECTED);
  ASSERT_EQ(signal_strategy_->GetError(), SignalStrategy::Error::NETWORK_ERROR);
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
      SendMessage(Property(&SignalingAddress::id, kFakeRemoteFtlId), _, _, _))
      .WillOnce(base::test::RunOnceCallback<2>(
          HttpStatus(HttpStatus::Code::UNAVAILABLE, "unavailable")));
  signal_strategy_->SendMessage(std::move(jingle_message));

  ASSERT_EQ(received_messages_.size(), 1u);
  auto& error_message = received_messages_[0];
  ASSERT_EQ(error_message->Attr(kQNameType), kIqTypeError);
  ASSERT_EQ(error_message->Attr(kQNameId), stanza_id);
  ASSERT_EQ(error_message->Attr(kQNameFrom), kFakeRemoteFtlId);
  ASSERT_EQ(error_message->Attr(kQNameTo), kFakeLocalFtlId);
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

  ASSERT_EQ(received_messages_.size(), 1u);
  EXPECT_EQ(received_messages_[0]->Attr(kQNameTo),
            std::string(kFakeLocalFtlId));
  EXPECT_EQ(received_messages_[0]->Attr(kQNameFrom),
            std::string(kFakeRemoteFtlId));
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

  EXPECT_CALL(*this, OnSignalingMessage(_, _))
      .WillOnce([&](const SignalingAddress& sender_address,
                    const JingleMessage& received_message) {
        SignalingAddress expected_address =
            SignalingAddress::CreateFtlSignalingAddress(
                kFakeRemoteUsername, kFakeRemoteRegistrationId);
        EXPECT_EQ(sender_address.id(), expected_address.id());

        auto xml = JingleMessageToXml(received_message);
        EXPECT_EQ(xml->Attr(kQNameTo), std::string(kFakeLocalFtlId));
        EXPECT_EQ(xml->Attr(kQNameFrom), std::string(kFakeRemoteFtlId));
        return true;
      });

  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  messaging_client_->OnMessage(remote_user_id, kFakeRemoteRegistrationId,
                               message);

  // Message has already been consumed in OnSignalingMessage().
  ASSERT_EQ(received_messages_.size(), 0u);
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

  ASSERT_EQ(received_messages_.size(), 0u);
}

TEST_F(FtlSignalStrategyTest, ReceiveStanza_RejectStanzaWithDtd) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  ftl::ChromotingMessage message;
  message.mutable_xmpp()->set_stanza(
      "<!DOCTYPE iq [ <!ENTITY xxe \"evil\"> ]></iq>");
  ftl::Id remote_user_id;
  remote_user_id.set_type(ftl::IdType_Type_EMAIL);
  remote_user_id.set_id(kFakeRemoteUsername);
  messaging_client_->OnMessage(remote_user_id, kFakeRemoteRegistrationId,
                               message);

  ASSERT_EQ(received_messages_.size(), 0u);
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
                  SignalingMessageMatches(message_payload), _, _))
      .WillOnce(base::test::RunOnceCallback<2>(HttpStatus::OK()));

  signal_strategy_->SendFtlMessage(
      SignalingAddress::CreateFtlSignalingAddress(kFakeRemoteUsername,
                                                  kFakeRemoteRegistrationId),
      std::move(message));
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
                  _, _, _))
      .WillOnce(base::test::RunOnceCallback<2>(
          HttpStatus(HttpStatus::Code::UNAUTHENTICATED, "unauthenticated")));

  ftl::ChromotingMessage message;
  signal_strategy_->SendFtlMessage(
      SignalingAddress::CreateFtlSignalingAddress(kFakeRemoteUsername,
                                                  kFakeRemoteRegistrationId),
      std::move(message));

  ASSERT_EQ(state_history_.size(), 3u);
  ASSERT_EQ(state_history_[0], SignalStrategy::State::CONNECTING);
  ASSERT_EQ(state_history_[1], SignalStrategy::State::CONNECTED);
  ASSERT_EQ(state_history_[2], SignalStrategy::State::DISCONNECTED);

  ASSERT_EQ(signal_strategy_->GetState(), SignalStrategy::State::DISCONNECTED);
  ASSERT_EQ(signal_strategy_->GetError(), SignalStrategy::Error::NETWORK_ERROR);
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
                  _, _, _))
      .WillOnce(base::test::RunOnceCallback<2>(
          HttpStatus(HttpStatus::Code::UNAVAILABLE, "unavailable")));

  ftl::ChromotingMessage message;
  signal_strategy_->SendFtlMessage(
      SignalingAddress::CreateFtlSignalingAddress(kFakeRemoteUsername,
                                                  kFakeRemoteRegistrationId),
      std::move(message));

  ASSERT_EQ(received_messages_.size(), 0u);
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

TEST_F(FtlSignalStrategyTest, ReceiveIncomingFtlMessage) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  ftl::ChromotingMessage message;
  message.mutable_echo()->set_message("echo");

  EXPECT_CALL(*this, OnIncomingFtlMessage(_, _)).WillOnce(Return(true));

  ftl::Id remote_user_id;
  remote_user_id.set_type(ftl::IdType_Type_EMAIL);
  remote_user_id.set_id(kFakeRemoteUsername);
  messaging_client_->OnMessage(remote_user_id, kFakeRemoteRegistrationId,
                               message);
}

// TODO: crbug.com/504910955 - Re-enable after iq_stanza is fixed.
TEST_F(FtlSignalStrategyTest, DISABLED_ReceiveIqStanzaOnly_Success) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  std::string stanza_id = signal_strategy_->GetNextId();
  ftl::ChromotingMessage message =
      CreateIqStanzaMessage(Direction::INCOMING, stanza_id);

  EXPECT_CALL(*this, OnSignalingMessage(_, _))
      .WillOnce([&](const SignalingAddress& sender_address,
                    const JingleMessage& received_message) {
        EXPECT_EQ(received_message.message_id, stanza_id);
        EXPECT_EQ(sender_address.id(), std::string(kFakeRemoteFtlId));
        return true;
      });

  ftl::Id remote_user_id;
  remote_user_id.set_type(ftl::IdType_Type_EMAIL);
  remote_user_id.set_id(kFakeRemoteUsername);
  messaging_client_->OnMessage(remote_user_id, kFakeRemoteRegistrationId,
                               message);
}

// TODO: crbug.com/504910955 - Re-enable after iq_stanza is fixed.
TEST_F(FtlSignalStrategyTest,
       DISABLED_ReceiveIqStanzaAndStanza_PreferIqStanza) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  std::string proto_stanza_id = "proto_id";
  ftl::ChromotingMessage message =
      CreateIqStanzaMessage(Direction::INCOMING, proto_stanza_id);

  std::string xml_stanza_id = "xml_id";
  auto xml_stanza = CreateXmlStanza(Direction::INCOMING, xml_stanza_id);
  message.mutable_xmpp()->set_stanza(xml_stanza->Str());

  EXPECT_CALL(*this, OnSignalingMessage(_, _))
      .WillOnce([&](const SignalingAddress& sender_address,
                    const JingleMessage& received_message) {
        EXPECT_EQ(received_message.message_id, proto_stanza_id);
        EXPECT_EQ(sender_address.id(), std::string(kFakeRemoteFtlId));
        return true;
      });

  ftl::Id remote_user_id;
  remote_user_id.set_type(ftl::IdType_Type_EMAIL);
  remote_user_id.set_id(kFakeRemoteUsername);
  messaging_client_->OnMessage(remote_user_id, kFakeRemoteRegistrationId,
                               message);
}

TEST_F(FtlSignalStrategyTest, ReceiveIqStanza_NoPayload) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  std::string stanza_id = signal_strategy_->GetNextId();
  ftl::ChromotingMessage message;
  auto* iq = message.mutable_xmpp()->mutable_iq_stanza();
  iq->set_id(stanza_id);
  iq->mutable_sender()->set_local_part(kFakeRemoteUsername);
  iq->mutable_sender()->set_resource_part(kFakeRemoteRegistrationId);
  iq->mutable_receiver()->set_local_part(kFakeLocalUsername);
  iq->mutable_receiver()->set_resource_part(kFakeLocalRegistrationId);

  // No payload set in iq_stanza.

  EXPECT_CALL(*this, OnSignalingMessage(_, _)).Times(0);
  EXPECT_CALL(*this, OnSignalingReply(_, _)).Times(0);

  ftl::Id remote_user_id;
  remote_user_id.set_type(ftl::IdType_Type_EMAIL);
  remote_user_id.set_id(kFakeRemoteUsername);
  messaging_client_->OnMessage(remote_user_id, kFakeRemoteRegistrationId,
                               message);
}

// TODO: crbug.com/504910955 - Re-enable after iq_stanza is fixed.
TEST_F(FtlSignalStrategyTest,
       DISABLED_SendReply_PopulatesBothStanzaAndIqStanza) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  JingleMessageReply reply;
  reply.to = SignalingAddress(kFakeRemoteFtlId);
  reply.message_id = signal_strategy_->GetNextId();
  reply.reply_type = JingleMessageReply::REPLY_RESULT;

  EXPECT_CALL(
      *messaging_client_,
      SendMessage(Property(&SignalingAddress::id, kFakeRemoteFtlId),
                  SignalingMessageMatches(kFakeRemoteFtlId, kFakeLocalFtlId), _,
                  _))
      .WillOnce(base::test::RunOnceCallback<2>(HttpStatus::OK()));

  signal_strategy_->SendReply(std::move(reply));
}

TEST_F(FtlSignalStrategyTest, SendMessage_SessionAcceptRequestsNotFoundRetry) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  JingleMessage message(SignalingAddress(kFakeRemoteFtlId), SessionAccept(),
                        "sid");
  message.message_id = "id";

  EXPECT_CALL(*messaging_client_, SendMessage(_, _, _, _))
      .WillOnce([&](const SignalingAddress&, ftl::ChromotingMessage&&,
                    FtlMessagingClient::DoneCallback callback,
                    scoped_refptr<const ProtobufHttpRequestConfig::RetryPolicy>
                        retry_policy) {
        ASSERT_TRUE(retry_policy);
        EXPECT_TRUE(retry_policy->retriable_error_codes.contains(
            HttpStatus::Code::NOT_FOUND));
        std::move(callback).Run(HttpStatus::OK());
      });

  signal_strategy_->SendMessage(std::move(message));
}

TEST_F(FtlSignalStrategyTest, SendReply_RequestsNotFoundRetry) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  JingleMessageReply reply;
  reply.to = SignalingAddress(kFakeRemoteFtlId);
  reply.message_id = "id";

  EXPECT_CALL(*messaging_client_, SendMessage(_, _, _, _))
      .WillOnce([&](const SignalingAddress&, ftl::ChromotingMessage&&,
                    FtlMessagingClient::DoneCallback callback,
                    scoped_refptr<const ProtobufHttpRequestConfig::RetryPolicy>
                        retry_policy) {
        ASSERT_TRUE(retry_policy);
        EXPECT_TRUE(retry_policy->retriable_error_codes.contains(
            HttpStatus::Code::NOT_FOUND));
        std::move(callback).Run(HttpStatus::OK());
      });

  signal_strategy_->SendReply(std::move(reply));
}

TEST_F(FtlSignalStrategyTest,
       SendMessage_OtherMessagesDoNotRequestNotFoundRetry) {
  ExpectGetOAuthTokenSucceedsWithFakeCreds();
  registration_manager_->ExpectSignInGaiaSucceeds();
  signal_strategy_->Connect();
  messaging_client_->AcceptReceivingMessages();

  JingleMessage message(SignalingAddress(kFakeRemoteFtlId), SessionInfo(),
                        "sid");
  message.message_id = "id";

  EXPECT_CALL(*messaging_client_, SendMessage(_, _, _, _))
      .WillOnce([&](const SignalingAddress&, ftl::ChromotingMessage&&,
                    FtlMessagingClient::DoneCallback callback,
                    scoped_refptr<const ProtobufHttpRequestConfig::RetryPolicy>
                        retry_policy) {
        ASSERT_FALSE(retry_policy);
        std::move(callback).Run(HttpStatus::OK());
      });

  signal_strategy_->SendMessage(std::move(message));
}

}  // namespace remoting
