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
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/base/http_status.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/base/protobuf_http_test_responder.h"
#include "remoting/base/scoped_protobuf_http_request.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"
#include "remoting/signaling/ftl_message_channel_strategy.h"
#include "remoting/signaling/ftl_services_context.h"
#include "remoting/signaling/registration_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using ::testing::_;
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

base::OnceCallback<void(const HttpStatus&)> CheckStatusThenQuitRunLoopCallback(
    const base::Location& from_here,
    HttpStatus::Code expected_status_code,
    base::RunLoop* run_loop) {
  return base::BindLambdaForTesting([=](const HttpStatus& status) {
    ASSERT_EQ(status.error_code(), expected_status_code)
        << "Incorrect status code. Location: " << from_here.ToString();
    run_loop->QuitWhenIdle();
  });
}

std::string GetChromotingMessageText(const ftl::InboxMessage& message) {
  EXPECT_EQ(message.message_type(),
            ftl::InboxMessage_MessageType_CHROMOTING_MESSAGE);
  ftl::ChromotingMessage chromoting_message;
  chromoting_message.ParseFromString(message.message());
  return chromoting_message.xmpp().stanza();
}

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
  FakeOAuthTokenGetter token_getter_{OAuthTokenGetter::Status::SUCCESS,
                                     OAuthTokenInfo()};
  std::unique_ptr<FtlMessagingClient> messaging_client_;

 private:
  base::test::TaskEnvironment task_environment_;
  MockRegistrationManager mock_registration_manager_;
};

void FtlMessagingClientTest::SetUp() {
  EXPECT_CALL(mock_registration_manager_, GetFtlAuthToken())
      .WillRepeatedly(Return("fake_auth_token"));
  messaging_client_ = std::unique_ptr<FtlMessagingClient>(
      new FtlMessagingClient(std::make_unique<ProtobufHttpClient>(
                                 kFakeServerEndpoint, &token_getter_,
                                 test_responder_.GetUrlLoaderFactory()),
                             &mock_registration_manager_,
                             /*signaling_tracker=*/nullptr,
                             std::make_unique<FtlMessageChannelStrategy>()));
}

void FtlMessagingClientTest::TearDown() {
  messaging_client_.reset();
}

TEST_F(FtlMessagingClientTest, TestSendMessage_Unauthenticated) {
  base::RunLoop run_loop;
  messaging_client_->SendMessage(
      kFakeReceiverId, kFakeSenderRegId, CreateXmppMessage(kMessage1Text),
      CheckStatusThenQuitRunLoopCallback(
          FROM_HERE, HttpStatus::Code::UNAUTHENTICATED, &run_loop));
  test_responder_.AddErrorToMostRecentRequestUrl(
      HttpStatus(HttpStatus::Code::UNAUTHENTICATED, "Unauthenticated"));
  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, TestSendMessage_SendOneMessageWithoutRegId) {
  base::RunLoop run_loop;
  messaging_client_->SendMessage(
      kFakeReceiverId, "", CreateXmppMessage(kMessage1Text),
      CheckStatusThenQuitRunLoopCallback(FROM_HERE, HttpStatus::Code::OK,
                                         &run_loop));

  ftl::InboxSendRequest request;
  ASSERT_TRUE(test_responder_.GetMostRecentRequestMessage(&request));
  EXPECT_EQ(request.dest_registration_ids_size(), 0);
  EXPECT_GE(request.time_to_live(), 0);
  EXPECT_EQ(request.dest_id().id(), kFakeReceiverId);
  EXPECT_FALSE(request.message().message_id().empty());
  EXPECT_EQ(GetChromotingMessageText(request.message()), kMessage1Text);

  test_responder_.AddResponseToMostRecentRequestUrl(ftl::InboxSendResponse());
  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, TestSendMessage_SendOneMessageWithRegId) {
  base::RunLoop run_loop;
  messaging_client_->SendMessage(
      kFakeReceiverId, kFakeSenderRegId, CreateXmppMessage(kMessage1Text),
      CheckStatusThenQuitRunLoopCallback(FROM_HERE, HttpStatus::Code::OK,
                                         &run_loop));

  ftl::InboxSendRequest request;
  ASSERT_TRUE(test_responder_.GetMostRecentRequestMessage(&request));
  EXPECT_EQ(request.dest_registration_ids_size(), 1);
  EXPECT_EQ(request.dest_registration_ids(0), kFakeSenderRegId);
  EXPECT_GE(request.time_to_live(), 0);
  EXPECT_EQ(request.dest_id().id(), kFakeReceiverId);
  EXPECT_FALSE(request.message().message_id().empty());
  EXPECT_EQ(GetChromotingMessageText(request.message()), kMessage1Text);

  test_responder_.AddResponseToMostRecentRequestUrl(ftl::InboxSendResponse());
  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, TestStartReceivingMessages_CallbacksForwarded) {
  base::RunLoop run_loop;

  base::MockCallback<base::OnceClosure> mock_on_ready_closure;
  EXPECT_CALL(mock_on_ready_closure, Run()).WillOnce(Return());

  messaging_client_->StartReceivingMessages(
      mock_on_ready_closure.Get(),
      CheckStatusThenQuitRunLoopCallback(FROM_HERE, HttpStatus::Code::UNKNOWN,
                                         &run_loop));

  ftl::ReceiveMessagesResponse start_of_batch;
  start_of_batch.mutable_start_of_batch();
  // Using `UNKNOWN` because it is a non-retriable error.
  test_responder_.AddStreamResponseToMostRecentRequestUrl(
      {&start_of_batch}, HttpStatus(HttpStatus::Code::UNKNOWN, ""));

  run_loop.Run();
}

TEST_F(FtlMessagingClientTest,
       TestStreamOpener_StreamsTwoMessagesThenClosedByServer) {
  base::RunLoop run_loop;

  base::MockCallback<FtlMessagingClient::MessageCallback> mock_on_incoming_msg;
  EXPECT_CALL(mock_on_incoming_msg, Run(_, _, _))
      .WillOnce([&](const ftl::Id&, const std::string&,
                    const ftl::ChromotingMessage& message) {
        EXPECT_EQ(message.xmpp().stanza(), kMessage1Text);
      })
      .WillOnce([&](const ftl::Id&, const std::string&,
                    const ftl::ChromotingMessage& message) {
        EXPECT_EQ(message.xmpp().stanza(), kMessage2Text);
        run_loop.Quit();
      });

  base::CallbackListSubscription subscription =
      messaging_client_->RegisterMessageCallback(mock_on_incoming_msg.Get());
  messaging_client_->StartReceivingMessages(base::DoNothing(),
                                            base::DoNothing());

  ftl::ReceiveMessagesResponse start_of_batch;
  start_of_batch.mutable_start_of_batch();

  ftl::ReceiveMessagesResponse inbox_message_1;
  *inbox_message_1.mutable_inbox_message() =
      CreateInboxMessage(kMessage1Id, kMessage1Text);

  ftl::ReceiveMessagesResponse inbox_message_2;
  *inbox_message_2.mutable_inbox_message() =
      CreateInboxMessage(kMessage2Id, kMessage2Text);

  test_responder_.AddStreamResponseToMostRecentRequestUrl(
      {&start_of_batch, &inbox_message_1, &inbox_message_2},
      HttpStatus(HttpStatus::Code::OK, ""));

  run_loop.Run();
}

TEST_F(FtlMessagingClientTest,
       TestOnMessageReceived_MessagePassedToSubscriberAndAcked) {
  base::RunLoop run_loop;

  base::MockCallback<FtlMessagingClient::MessageCallback> mock_on_incoming_msg;
  EXPECT_CALL(mock_on_incoming_msg, Run(IsFakeSenderId(), kFakeSenderRegId,
                                        StanzaTextMatches(kMessage1Text)))
      .WillOnce(Return());

  base::CallbackListSubscription subscription =
      messaging_client_->RegisterMessageCallback(mock_on_incoming_msg.Get());
  messaging_client_->StartReceivingMessages(base::DoNothing(),
                                            base::DoNothing());

  ftl::ReceiveMessagesResponse start_of_batch;
  start_of_batch.mutable_start_of_batch();

  ftl::ReceiveMessagesResponse inbox_message;
  *inbox_message.mutable_inbox_message() =
      CreateInboxMessage(kMessage1Id, kMessage1Text);

  test_responder_.AddStreamResponseToMostRecentRequestUrl(
      {&start_of_batch, &inbox_message}, HttpStatus(HttpStatus::Code::OK, ""));

  EXPECT_CALL(test_responder_.GetMockInterceptor(), Run(_))
      .WillRepeatedly([&](const network::ResourceRequest& request) {
        ftl::BatchAckMessagesRequest message;
        ProtobufHttpTestResponder::ParseRequestMessage(request, &message);
        if (message.message_ids_size() < 1) {
          // Ignore non-batch messages.
          return;
        }
        EXPECT_EQ(message.message_ids_size(), 1);
        EXPECT_EQ(message.message_ids(0), kMessage1Id);
        run_loop.Quit();
      });

  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, ReceivedDuplicatedMessage_AckAndDrop) {
  base::RunLoop run_loop;

  base::MockCallback<FtlMessagingClient::MessageCallback> mock_on_incoming_msg;
  EXPECT_CALL(mock_on_incoming_msg, Run(IsFakeSenderId(), kFakeSenderRegId, _))
      .WillOnce([](const ftl::Id&, const std::string&,
                   const ftl::ChromotingMessage& message) {
        EXPECT_EQ(message.xmpp().stanza(), kMessage1Text);
      })
      .WillOnce([](const ftl::Id&, const std::string&,
                   const ftl::ChromotingMessage& message) {
        EXPECT_EQ(message.xmpp().stanza(), kMessage2Text);
      });

  int ack_count = 0;
  EXPECT_CALL(test_responder_.GetMockInterceptor(), Run(_))
      .WillRepeatedly([&](const network::ResourceRequest& request) {
        ftl::BatchAckMessagesRequest message;
        ProtobufHttpTestResponder::ParseRequestMessage(request, &message);
        if (message.message_ids_size() < 1) {
          // Ignore non-batch messages.
          return;
        }
        ack_count++;
        EXPECT_EQ(message.message_ids_size(), 1);
        if (ack_count == 1) {
          EXPECT_EQ(message.message_ids(0), kMessage1Id);
        } else if (ack_count == 2) {
          EXPECT_EQ(message.message_ids(0), kMessage1Id);
        } else if (ack_count == 3) {
          EXPECT_EQ(message.message_ids(0), kMessage2Id);
          run_loop.Quit();
        }
      });

  base::CallbackListSubscription subscription =
      messaging_client_->RegisterMessageCallback(mock_on_incoming_msg.Get());
  messaging_client_->StartReceivingMessages(base::DoNothing(),
                                            base::DoNothing());

  ftl::ReceiveMessagesResponse start_of_batch;
  start_of_batch.mutable_start_of_batch();

  ftl::ReceiveMessagesResponse inbox_message_1;
  *inbox_message_1.mutable_inbox_message() =
      CreateInboxMessage(kMessage1Id, kMessage1Text);

  ftl::ReceiveMessagesResponse inbox_message_1_resend;
  *inbox_message_1_resend.mutable_inbox_message() =
      CreateInboxMessage(kMessage1Id, kMessage1Text);

  ftl::ReceiveMessagesResponse inbox_message_2;
  *inbox_message_2.mutable_inbox_message() =
      CreateInboxMessage(kMessage2Id, kMessage2Text);

  test_responder_.AddStreamResponseToMostRecentRequestUrl(
      {&start_of_batch, &inbox_message_1, &inbox_message_1_resend,
       &inbox_message_2},
      HttpStatus(HttpStatus::Code::OK, ""));

  run_loop.Run();
}

}  // namespace remoting
