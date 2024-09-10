// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_messaging_client.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/protobuf_http_test_responder.h"
#include "remoting/base/scoped_protobuf_http_request.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"
#include "remoting/signaling/ftl_services_context.h"
#include "remoting/signaling/message_reception_channel.h"
#include "remoting/signaling/registration_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::Return;
using ::testing::Truly;

constexpr char kFakeServerEndpoint[] = "test.com";
constexpr char kFakeSenderId[] = "fake_sender@gmail.com";
constexpr char kFakeSenderRegId[] = "fake_sender_reg_id";
constexpr char kFakeReceiverId[] = "fake_receiver@gmail.com";
constexpr char kMessage1Id[] = "msg_1";
constexpr char kMessage2Id[] = "msg_2";
constexpr char kMessage1Text[] = "Message 1";
constexpr char kMessage2Text[] = "Message 2";

MATCHER(IsFakeSenderId, "") {
  return arg.id() == kFakeSenderId;
}

ftl::ChromotingMessage CreateXmppMessage(const std::string& message_text) {
  ftl::ChromotingMessage crd_message;
  crd_message.mutable_xmpp()->set_stanza(message_text);
  return crd_message;
}

ftl::InboxMessage CreateInboxMessage(const std::string& message_id,
                                     const std::string& message_text) {
  ftl::InboxMessage message;
  message.mutable_sender_id()->set_id(kFakeSenderId);
  message.mutable_receiver_id()->set_id(kFakeReceiverId);
  message.set_sender_registration_id(kFakeSenderRegId);
  message.set_message_type(ftl::InboxMessage_MessageType_CHROMOTING_MESSAGE);
  message.set_message_id(message_id);
  ftl::ChromotingMessage crd_message = CreateXmppMessage(message_text);
  std::string serialized_message;
  bool succeeded = crd_message.SerializeToString(&serialized_message);
  EXPECT_TRUE(succeeded);
  message.set_message(serialized_message);
  return message;
}

base::OnceCallback<void(const ProtobufHttpStatus&)>
CheckStatusThenQuitRunLoopCallback(
    const base::Location& from_here,
    ProtobufHttpStatus::Code expected_status_code,
    base::RunLoop* run_loop) {
  return base::BindLambdaForTesting([=](const ProtobufHttpStatus& status) {
    ASSERT_EQ(expected_status_code, status.error_code())
        << "Incorrect status code. Location: " << from_here.ToString();
    run_loop->QuitWhenIdle();
  });
}

std::string GetChromotingMessageText(const ftl::InboxMessage& message) {
  EXPECT_EQ(ftl::InboxMessage_MessageType_CHROMOTING_MESSAGE,
            message.message_type());
  ftl::ChromotingMessage chromoting_message;
  chromoting_message.ParseFromString(message.message());
  return chromoting_message.xmpp().stanza();
}

class MockMessageReceptionChannel : public MessageReceptionChannel {
 public:
  MockMessageReceptionChannel() = default;

  MockMessageReceptionChannel(const MockMessageReceptionChannel&) = delete;
  MockMessageReceptionChannel& operator=(const MockMessageReceptionChannel&) =
      delete;

  ~MockMessageReceptionChannel() override = default;

  // MessageReceptionChannel implementations.
  void Initialize(const StreamOpener& stream_opener,
                  const MessageCallback& on_incoming_msg) override {
    stream_opener_ = stream_opener;
    on_incoming_msg_ = on_incoming_msg;
  }

  MOCK_METHOD2(StartReceivingMessages, void(base::OnceClosure, DoneCallback));
  MOCK_METHOD0(StopReceivingMessages, void());
  MOCK_CONST_METHOD0(IsReceivingMessages, bool());

  StreamOpener* stream_opener() { return &stream_opener_; }

  MessageCallback* on_incoming_msg() { return &on_incoming_msg_; }

 private:
  StreamOpener stream_opener_;
  MessageCallback on_incoming_msg_;
};

class MockRegistrationManager : public RegistrationManager {
 public:
  MockRegistrationManager() = default;
  ~MockRegistrationManager() override = default;

  MOCK_METHOD1(SignInGaia, void(DoneCallback));
  MOCK_METHOD0(SignOut, void());
  MOCK_CONST_METHOD0(IsSignedIn, bool());
  MOCK_CONST_METHOD0(GetRegistrationId, std::string());
  MOCK_CONST_METHOD0(GetFtlAuthToken, std::string());
};

decltype(auto) StanzaTextMatches(const std::string& expected_stanza) {
  return Truly([=](const ftl::ChromotingMessage& message) {
    return expected_stanza == message.xmpp().stanza();
  });
}

}  // namespace

class FtlMessagingClientTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  ProtobufHttpTestResponder test_responder_;
  FakeOAuthTokenGetter token_getter_{OAuthTokenGetter::Status::SUCCESS, "", "",
                                     ""};
  std::unique_ptr<FtlMessagingClient> messaging_client_;
  raw_ptr<MockMessageReceptionChannel> mock_message_reception_channel_;

 private:
  base::test::TaskEnvironment task_environment_;
  MockRegistrationManager mock_registration_manager_;
};

void FtlMessagingClientTest::SetUp() {
  EXPECT_CALL(mock_registration_manager_, GetFtlAuthToken())
      .WillRepeatedly(Return("fake_auth_token"));
  auto channel = std::make_unique<MockMessageReceptionChannel>();
  mock_message_reception_channel_ = channel.get();
  messaging_client_ = std::unique_ptr<FtlMessagingClient>(
      new FtlMessagingClient(std::make_unique<ProtobufHttpClient>(
                                 kFakeServerEndpoint, &token_getter_,
                                 test_responder_.GetUrlLoaderFactory()),
                             &mock_registration_manager_, std::move(channel)));
}

void FtlMessagingClientTest::TearDown() {
  mock_message_reception_channel_ = nullptr;
  messaging_client_.reset();
}

TEST_F(FtlMessagingClientTest, TestSendMessage_Unauthenticated) {
  base::RunLoop run_loop;
  messaging_client_->SendMessage(
      kFakeReceiverId, kFakeSenderRegId, CreateXmppMessage(kMessage1Text),
      CheckStatusThenQuitRunLoopCallback(
          FROM_HERE, ProtobufHttpStatus::Code::UNAUTHENTICATED, &run_loop));
  test_responder_.AddErrorToMostRecentRequestUrl(ProtobufHttpStatus(
      ProtobufHttpStatus::Code::UNAUTHENTICATED, "Unauthenticated"));
  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, TestSendMessage_SendOneMessageWithoutRegId) {
  base::RunLoop run_loop;
  messaging_client_->SendMessage(
      kFakeReceiverId, "", CreateXmppMessage(kMessage1Text),
      CheckStatusThenQuitRunLoopCallback(
          FROM_HERE, ProtobufHttpStatus::Code::OK, &run_loop));

  ftl::InboxSendRequest request;
  ASSERT_TRUE(test_responder_.GetMostRecentRequestMessage(&request));
  EXPECT_EQ(0, request.dest_registration_ids_size());
  EXPECT_LT(0, request.time_to_live());
  EXPECT_EQ(kFakeReceiverId, request.dest_id().id());
  EXPECT_FALSE(request.message().message_id().empty());
  EXPECT_EQ(kMessage1Text, GetChromotingMessageText(request.message()));

  test_responder_.AddResponseToMostRecentRequestUrl(ftl::InboxSendResponse());
  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, TestSendMessage_SendOneMessageWithRegId) {
  base::RunLoop run_loop;
  messaging_client_->SendMessage(
      kFakeReceiverId, kFakeSenderRegId, CreateXmppMessage(kMessage1Text),
      CheckStatusThenQuitRunLoopCallback(
          FROM_HERE, ProtobufHttpStatus::Code::OK, &run_loop));

  ftl::InboxSendRequest request;
  ASSERT_TRUE(test_responder_.GetMostRecentRequestMessage(&request));
  EXPECT_EQ(1, request.dest_registration_ids_size());
  EXPECT_EQ(kFakeSenderRegId, request.dest_registration_ids(0));
  EXPECT_LT(0, request.time_to_live());
  EXPECT_EQ(kFakeReceiverId, request.dest_id().id());
  EXPECT_FALSE(request.message().message_id().empty());
  EXPECT_EQ(kMessage1Text, GetChromotingMessageText(request.message()));

  test_responder_.AddResponseToMostRecentRequestUrl(ftl::InboxSendResponse());
  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, TestStartReceivingMessages_CallbacksForwarded) {
  base::RunLoop run_loop;

  EXPECT_CALL(*mock_message_reception_channel_, StartReceivingMessages(_, _))
      .WillOnce(Invoke([&](base::OnceClosure on_ready,
                           FtlMessagingClient::DoneCallback on_closed) {
        std::move(on_ready).Run();
        std::move(on_closed).Run(
            ProtobufHttpStatus(ProtobufHttpStatus::Code::UNAUTHENTICATED, ""));
      }));

  base::MockCallback<base::OnceClosure> mock_on_ready_closure;
  EXPECT_CALL(mock_on_ready_closure, Run()).WillOnce(Return());

  messaging_client_->StartReceivingMessages(
      mock_on_ready_closure.Get(),
      base::BindLambdaForTesting([&](const ProtobufHttpStatus& status) {
        ASSERT_EQ(ProtobufHttpStatus::Code::UNAUTHENTICATED,
                  status.error_code());
        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, TestStopReceivingMessages_CallForwarded) {
  EXPECT_CALL(*mock_message_reception_channel_, StopReceivingMessages())
      .WillOnce(Return());
  messaging_client_->StopReceivingMessages();
}

TEST_F(FtlMessagingClientTest,
       TestStreamOpener_StreamsTwoMessagesThenCloseByServer) {
  base::RunLoop run_loop;

  std::unique_ptr<ScopedProtobufHttpRequest> scoped_stream;

  ftl::ReceiveMessagesResponse response_1;
  response_1.mutable_inbox_message()->set_message_id(kMessage1Id);
  ftl::ReceiveMessagesResponse response_2;
  response_2.mutable_pong();

  base::MockCallback<base::RepeatingCallback<void(
      std::unique_ptr<ftl::ReceiveMessagesResponse>)>>
      mock_on_incoming_msg;
  EXPECT_CALL(mock_on_incoming_msg, Run(_))
      .WillOnce([&](std::unique_ptr<ftl::ReceiveMessagesResponse> response) {
        ASSERT_EQ(kMessage1Id, response->inbox_message().message_id());
      })
      .WillOnce([&](std::unique_ptr<ftl::ReceiveMessagesResponse> response) {
        ASSERT_TRUE(response->has_pong());
      });

  base::MockCallback<base::OnceClosure> on_channel_ready;
  EXPECT_CALL(on_channel_ready, Run()).Times(1);

  scoped_stream = mock_message_reception_channel_->stream_opener()->Run(
      on_channel_ready.Get(), mock_on_incoming_msg.Get(),
      CheckStatusThenQuitRunLoopCallback(
          FROM_HERE, ProtobufHttpStatus::Code::OK, &run_loop));

  test_responder_.AddStreamResponseToMostRecentRequestUrl(
      {&response_1, &response_2}, ProtobufHttpStatus::OK());

  run_loop.Run();
}

TEST_F(FtlMessagingClientTest,
       TestOnMessageReceived_MessagePassedToSubscriberAndAcked) {
  base::RunLoop run_loop;
  base::MockCallback<FtlMessagingClient::MessageCallback> mock_on_incoming_msg;
  EXPECT_CALL(mock_on_incoming_msg, Run(IsFakeSenderId(), kFakeSenderRegId,
                                        StanzaTextMatches(kMessage1Text)))
      .WillOnce([&](const ftl::Id&, const std::string&,
                    const ftl::ChromotingMessage&) { run_loop.Quit(); });

  auto subscription =
      messaging_client_->RegisterMessageCallback(mock_on_incoming_msg.Get());
  ftl::InboxMessage message = CreateInboxMessage(kMessage1Id, kMessage1Text);
  mock_message_reception_channel_->on_incoming_msg()->Run(message);

  ftl::BatchAckMessagesRequest request;
  ASSERT_TRUE(test_responder_.GetMostRecentRequestMessage(&request));
  EXPECT_EQ(1, request.message_ids_size());
  EXPECT_EQ(kMessage1Id, request.message_ids(0));
  test_responder_.AddResponseToMostRecentRequestUrl(
      ftl::BatchAckMessagesResponse());

  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, ReceivedDuplicatedMessage_AckAndDrop) {
  base::RunLoop run_loop;

  base::MockCallback<FtlMessagingClient::MessageCallback> mock_on_incoming_msg;
  EXPECT_CALL(mock_on_incoming_msg, Run(IsFakeSenderId(), kFakeSenderRegId, _))
      .WillOnce([](const ftl::Id&, const std::string&,
                   const ftl::ChromotingMessage& message) {
        EXPECT_EQ(kMessage1Text, message.xmpp().stanza());
      })
      .WillOnce([](const ftl::Id&, const std::string&,
                   const ftl::ChromotingMessage& message) {
        EXPECT_EQ(kMessage2Text, message.xmpp().stanza());
      });

  EXPECT_CALL(test_responder_.GetMockInterceptor(), Run(_))
      .WillOnce([](const network::ResourceRequest& request) {
        ftl::BatchAckMessagesRequest message;
        ProtobufHttpTestResponder::ParseRequestMessage(request, &message);
        EXPECT_EQ(1, message.message_ids_size());
        EXPECT_EQ(kMessage1Id, message.message_ids(0));
      })
      .WillOnce([](const network::ResourceRequest& request) {
        ftl::BatchAckMessagesRequest message;
        ProtobufHttpTestResponder::ParseRequestMessage(request, &message);
        EXPECT_EQ(1, message.message_ids_size());
        EXPECT_EQ(kMessage1Id, message.message_ids(0));
      })
      .WillOnce([&](const network::ResourceRequest& request) {
        ftl::BatchAckMessagesRequest message;
        ProtobufHttpTestResponder::ParseRequestMessage(request, &message);
        EXPECT_EQ(1, message.message_ids_size());
        EXPECT_EQ(kMessage2Id, message.message_ids(0));
        run_loop.Quit();
      });

  auto subscription =
      messaging_client_->RegisterMessageCallback(mock_on_incoming_msg.Get());
  ftl::InboxMessage message_1 = CreateInboxMessage(kMessage1Id, kMessage1Text);
  mock_message_reception_channel_->on_incoming_msg()->Run(message_1);

  ftl::InboxMessage message_2 = CreateInboxMessage(kMessage1Id, kMessage1Text);
  mock_message_reception_channel_->on_incoming_msg()->Run(message_2);

  ftl::InboxMessage message_3 = CreateInboxMessage(kMessage2Id, kMessage2Text);
  mock_message_reception_channel_->on_incoming_msg()->Run(message_3);

  run_loop.Run();
}

}  // namespace remoting
