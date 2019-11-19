// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_messaging_client.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "remoting/base/grpc_support/grpc_async_executor.h"
#include "remoting/base/grpc_support/scoped_grpc_server_stream.h"
#include "remoting/base/grpc_test_support/grpc_async_test_server.h"
#include "remoting/base/grpc_test_support/grpc_test_util.h"
#include "remoting/proto/ftl/v1/ftl_services.grpc.pb.h"
#include "remoting/signaling/ftl_grpc_context.h"
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

using PullMessagesResponder =
    test::GrpcServerResponder<ftl::PullMessagesResponse>;
using AckMessagesResponder =
    test::GrpcServerResponder<ftl::AckMessagesResponse>;

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

FtlMessagingClient::MessageCallback CreateNotReachedMessageCallback() {
  return base::BindRepeating(
      [](const ftl::Id& sender_id, const std::string& sender_registration_id,
         const ftl::ChromotingMessage& message) { NOTREACHED(); });
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

  DISALLOW_COPY_AND_ASSIGN(MockMessageReceptionChannel);
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
  using Messaging =
      google::internal::communications::instantmessaging::v1::Messaging;

  // Calls are scheduled sequentially and handled on the server thread.
  void ServerWaitAndRespondToPullMessagesRequest(
      const ftl::PullMessagesResponse& response,
      const grpc::Status& status);
  void ServerWaitAndRespondToInboxSendRequest(
      base::OnceCallback<grpc::Status(const ftl::InboxSendRequest&)> handler,
      base::OnceClosure on_done);
  void ServerWaitAndRespondToAckMessagesRequest(
      base::OnceCallback<grpc::Status(const ftl::AckMessagesRequest&)> handler,
      base::OnceClosure on_done);

  std::unique_ptr<test::GrpcAsyncTestServer> server_;
  scoped_refptr<base::SequencedTaskRunner> server_task_runner_;
  std::unique_ptr<FtlMessagingClient> messaging_client_;
  MockMessageReceptionChannel* mock_message_reception_channel_;

 private:
  base::test::TaskEnvironment task_environment_;
  MockRegistrationManager mock_registration_manager_;
};

void FtlMessagingClientTest::SetUp() {
  server_task_runner_ =
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock()});
  server_ = std::make_unique<test::GrpcAsyncTestServer>(
      std::make_unique<Messaging::AsyncService>());
  FtlGrpcContext::SetChannelForTesting(server_->CreateInProcessChannel());
  EXPECT_CALL(mock_registration_manager_, GetFtlAuthToken())
      .WillRepeatedly(Return("fake_auth_token"));
  auto channel = std::make_unique<MockMessageReceptionChannel>();
  mock_message_reception_channel_ = channel.get();
  messaging_client_ = std::unique_ptr<FtlMessagingClient>(
      new FtlMessagingClient(std::make_unique<GrpcAsyncExecutor>(),
                             &mock_registration_manager_, std::move(channel)));
}

void FtlMessagingClientTest::TearDown() {
  messaging_client_.reset();
  FtlGrpcContext::SetChannelForTesting(nullptr);
  server_task_runner_->DeleteSoon(FROM_HERE, std::move(server_));
}

void FtlMessagingClientTest::ServerWaitAndRespondToPullMessagesRequest(
    const ftl::PullMessagesResponse& response,
    const grpc::Status& status) {
  server_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const ftl::PullMessagesResponse& response,
             const grpc::Status& status, test::GrpcAsyncTestServer* server) {
            ftl::PullMessagesRequest request;
            auto responder = server->HandleRequest(
                &Messaging::AsyncService::RequestPullMessages, &request);
            responder->Respond(response, status);
          },
          response, status, server_.get()));
}

void FtlMessagingClientTest::ServerWaitAndRespondToInboxSendRequest(
    base::OnceCallback<grpc::Status(const ftl::InboxSendRequest&)> handler,
    base::OnceClosure on_done) {
  server_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceCallback<grpc::Status(const ftl::InboxSendRequest&)>
                 handler,
             test::GrpcAsyncTestServer* server) {
            ftl::InboxSendRequest request;
            auto responder = server->HandleRequest(
                &Messaging::AsyncService::RequestSendMessage, &request);
            grpc::Status status = std::move(handler).Run(request);
            responder->Respond(ftl::InboxSendResponse(), status);
          },
          std::move(handler), server_.get()),
      std::move(on_done));
}

void FtlMessagingClientTest::ServerWaitAndRespondToAckMessagesRequest(
    base::OnceCallback<grpc::Status(const ftl::AckMessagesRequest&)> handler,
    base::OnceClosure on_done) {
  server_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceCallback<grpc::Status(const ftl::AckMessagesRequest&)>
                 handler,
             test::GrpcAsyncTestServer* server) {
            ftl::AckMessagesRequest request;
            auto responder = server->HandleRequest(
                &Messaging::AsyncService::RequestAckMessages, &request);
            grpc::Status status = std::move(handler).Run(request);
            responder->Respond(ftl::AckMessagesResponse(), status);
          },
          std::move(handler), server_.get()),
      std::move(on_done));
}

TEST_F(FtlMessagingClientTest, TestPullMessages_ReturnsNoMessage) {
  base::RunLoop run_loop;
  auto subscription = messaging_client_->RegisterMessageCallback(
      CreateNotReachedMessageCallback());
  messaging_client_->PullMessages(test::CheckStatusThenQuitRunLoopCallback(
      FROM_HERE, grpc::StatusCode::OK, &run_loop));
  ServerWaitAndRespondToPullMessagesRequest(ftl::PullMessagesResponse(),
                                            grpc::Status::OK);
  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, TestPullMessages_Unauthenticated) {
  base::RunLoop run_loop;
  auto subscription = messaging_client_->RegisterMessageCallback(
      CreateNotReachedMessageCallback());
  messaging_client_->PullMessages(test::CheckStatusThenQuitRunLoopCallback(
      FROM_HERE, grpc::StatusCode::UNAUTHENTICATED, &run_loop));
  ServerWaitAndRespondToPullMessagesRequest(
      ftl::PullMessagesResponse(),
      grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Unauthenticated"));
  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, TestPullMessages_IgnoresMessageWithoutRegId) {
  base::RunLoop run_loop;

  auto subscription = messaging_client_->RegisterMessageCallback(
      CreateNotReachedMessageCallback());

  messaging_client_->PullMessages(test::CheckStatusThenQuitRunLoopCallback(
      FROM_HERE, grpc::StatusCode::OK, &run_loop));

  ftl::PullMessagesResponse response;
  ftl::InboxMessage* message = response.add_messages();
  *message = CreateInboxMessage(kMessage1Id, kMessage1Text);
  message->clear_sender_registration_id();
  ServerWaitAndRespondToPullMessagesRequest(response, grpc::Status::OK);
  ServerWaitAndRespondToAckMessagesRequest(
      base::BindLambdaForTesting([&](const ftl::AckMessagesRequest& request) {
        EXPECT_EQ(1, request.messages_size());
        EXPECT_EQ(kFakeReceiverId, request.messages(0).receiver_id().id());
        EXPECT_EQ(kMessage1Id, request.messages(0).message_id());
        return grpc::Status::OK;
      }),
      base::DoNothing());
  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, TestPullMessages_IgnoresUnknownMessageType) {
  base::RunLoop run_loop;

  auto subscription = messaging_client_->RegisterMessageCallback(
      CreateNotReachedMessageCallback());

  messaging_client_->PullMessages(test::CheckStatusThenQuitRunLoopCallback(
      FROM_HERE, grpc::StatusCode::OK, &run_loop));

  ftl::PullMessagesResponse response;
  ftl::InboxMessage* message = response.add_messages();
  *message = CreateInboxMessage(kMessage1Id, kMessage1Text);
  message->set_message_type(ftl::InboxMessage_MessageType_UNKNOWN);
  ServerWaitAndRespondToPullMessagesRequest(response, grpc::Status::OK);
  ServerWaitAndRespondToAckMessagesRequest(
      base::BindLambdaForTesting([&](const ftl::AckMessagesRequest& request) {
        EXPECT_EQ(1, request.messages_size());
        EXPECT_EQ(kFakeReceiverId, request.messages(0).receiver_id().id());
        EXPECT_EQ(kMessage1Id, request.messages(0).message_id());
        return grpc::Status::OK;
      }),
      base::DoNothing());
  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, TestPullMessages_ReturnsAndAcksTwoMessages) {
  base::RunLoop run_loop;

  base::MockCallback<FtlMessagingClient::MessageCallback> mock_on_incoming_msg;

  EXPECT_CALL(mock_on_incoming_msg, Run(IsFakeSenderId(), kFakeSenderRegId,
                                        StanzaTextMatches(kMessage1Text)))
      .WillOnce(Return());
  EXPECT_CALL(mock_on_incoming_msg, Run(IsFakeSenderId(), kFakeSenderRegId,
                                        StanzaTextMatches(kMessage2Text)))
      .WillOnce(Return());

  auto subscription =
      messaging_client_->RegisterMessageCallback(mock_on_incoming_msg.Get());

  messaging_client_->PullMessages(test::CheckStatusThenQuitRunLoopCallback(
      FROM_HERE, grpc::StatusCode::OK, &run_loop));

  ftl::PullMessagesResponse pull_messages_response;
  ftl::InboxMessage* message = pull_messages_response.add_messages();
  *message = CreateInboxMessage(kMessage1Id, kMessage1Text);
  message = pull_messages_response.add_messages();
  *message = CreateInboxMessage(kMessage2Id, kMessage2Text);
  ServerWaitAndRespondToPullMessagesRequest(pull_messages_response,
                                            grpc::Status::OK);

  ServerWaitAndRespondToAckMessagesRequest(
      base::BindLambdaForTesting([&](const ftl::AckMessagesRequest& request) {
        EXPECT_EQ(2, request.messages_size());
        EXPECT_EQ(kFakeReceiverId, request.messages(0).receiver_id().id());
        EXPECT_EQ(kFakeReceiverId, request.messages(1).receiver_id().id());
        EXPECT_EQ(kMessage1Id, request.messages(0).message_id());
        EXPECT_EQ(kMessage2Id, request.messages(1).message_id());
        return grpc::Status::OK;
      }),
      base::DoNothing());

  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, TestSendMessage_Unauthenticated) {
  base::RunLoop run_loop;
  messaging_client_->SendMessage(
      kFakeReceiverId, kFakeSenderRegId, CreateXmppMessage(kMessage1Text),
      test::CheckStatusThenQuitRunLoopCallback(
          FROM_HERE, grpc::StatusCode::UNAUTHENTICATED, &run_loop));
  ServerWaitAndRespondToInboxSendRequest(
      base::BindOnce([](const ftl::InboxSendRequest&) {
        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED,
                            "Unauthenticated");
      }),
      base::DoNothing());
  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, TestSendMessage_SendOneMessageWithoutRegId) {
  base::RunLoop run_loop;
  messaging_client_->SendMessage(
      kFakeReceiverId, "", CreateXmppMessage(kMessage1Text),
      test::CheckStatusThenQuitRunLoopCallback(FROM_HERE, grpc::StatusCode::OK,
                                               &run_loop));
  ServerWaitAndRespondToInboxSendRequest(
      base::BindOnce([](const ftl::InboxSendRequest& request) {
        EXPECT_EQ(0, request.dest_registration_ids_size());
        EXPECT_LT(0, request.time_to_live());
        EXPECT_EQ(kFakeReceiverId, request.dest_id().id());
        EXPECT_FALSE(request.message().message_id().empty());
        EXPECT_EQ(kMessage1Text, GetChromotingMessageText(request.message()));
        return grpc::Status::OK;
      }),
      base::DoNothing());
  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, TestSendMessage_SendOneMessageWithRegId) {
  base::RunLoop run_loop;
  messaging_client_->SendMessage(
      kFakeReceiverId, kFakeSenderRegId, CreateXmppMessage(kMessage1Text),
      test::CheckStatusThenQuitRunLoopCallback(FROM_HERE, grpc::StatusCode::OK,
                                               &run_loop));
  ServerWaitAndRespondToInboxSendRequest(
      base::BindOnce([](const ftl::InboxSendRequest& request) {
        EXPECT_EQ(1, request.dest_registration_ids_size());
        EXPECT_EQ(kFakeSenderRegId, request.dest_registration_ids(0));
        EXPECT_LT(0, request.time_to_live());
        EXPECT_EQ(kFakeReceiverId, request.dest_id().id());
        EXPECT_FALSE(request.message().message_id().empty());
        EXPECT_EQ(kMessage1Text, GetChromotingMessageText(request.message()));
        return grpc::Status::OK;
      }),
      base::DoNothing());
  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, TestStartReceivingMessages_CallbacksForwarded) {
  base::RunLoop run_loop;

  EXPECT_CALL(*mock_message_reception_channel_, StartReceivingMessages(_, _))
      .WillOnce(Invoke([&](base::OnceClosure on_ready,
                           FtlMessagingClient::DoneCallback on_closed) {
        std::move(on_ready).Run();
        std::move(on_closed).Run(
            grpc::Status(grpc::StatusCode::UNAUTHENTICATED, ""));
      }));

  base::MockCallback<base::OnceClosure> mock_on_ready_closure;
  EXPECT_CALL(mock_on_ready_closure, Run()).WillOnce(Return());

  messaging_client_->StartReceivingMessages(
      mock_on_ready_closure.Get(),
      base::BindLambdaForTesting([&](const grpc::Status& status) {
        ASSERT_EQ(grpc::StatusCode::UNAUTHENTICATED, status.error_code());
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
       TestStreamOpener_StreamsTwoMessagesThenCloseByClient) {
  base::RunLoop run_loop;

  std::unique_ptr<ScopedGrpcServerStream> scoped_stream;

  ftl::ReceiveMessagesResponse response_1;
  response_1.mutable_inbox_message()->set_message_id(kMessage1Id);
  ftl::ReceiveMessagesResponse response_2;
  response_2.mutable_pong();

  base::MockCallback<base::RepeatingClosure> exit_runloop_when_called_twice;
  EXPECT_CALL(exit_runloop_when_called_twice, Run())
      .WillOnce(Return())
      .WillOnce([&]() {
        // Whoever calls last quits the run loop.
        run_loop.QuitWhenIdle();
      });

  server_task_runner_->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        ftl::ReceiveMessagesRequest request;
        // This blocks the server thread until the client opens the stream.
        auto responder = server_->HandleStreamRequest(
            &Messaging::AsyncService::RequestReceiveMessages, &request);
        responder->SendMessage(response_1);
        ASSERT_TRUE(responder->WaitForSendMessageResult());
        responder->SendMessage(response_2);
        ASSERT_TRUE(responder->WaitForSendMessageResult());
      }),
      exit_runloop_when_called_twice.Get());

  base::MockCallback<
      base::RepeatingCallback<void(const ftl::ReceiveMessagesResponse&)>>
      mock_on_incoming_msg;
  EXPECT_CALL(mock_on_incoming_msg, Run(_))
      .WillOnce([&](const ftl::ReceiveMessagesResponse& response) {
        ASSERT_EQ(kMessage1Id, response.inbox_message().message_id());
      })
      .WillOnce([&](const ftl::ReceiveMessagesResponse& response) {
        ASSERT_TRUE(response.has_pong());
        DCHECK(scoped_stream);
        scoped_stream.reset();
        exit_runloop_when_called_twice.Get().Run();
      });

  base::MockCallback<base::OnceClosure> on_channel_ready;
  EXPECT_CALL(on_channel_ready, Run()).Times(1);

  scoped_stream = mock_message_reception_channel_->stream_opener()->Run(
      on_channel_ready.Get(), mock_on_incoming_msg.Get(),
      base::BindOnce([](const grpc::Status&) { NOTREACHED(); }));

  run_loop.Run();
}

TEST_F(FtlMessagingClientTest,
       TestOnMessageReceived_MessagePassedToSubscriberAndAcked) {
  base::RunLoop run_loop;

  ftl::InboxMessage message = CreateInboxMessage(kMessage1Id, kMessage1Text);
  mock_message_reception_channel_->on_incoming_msg()->Run(message);

  ServerWaitAndRespondToAckMessagesRequest(
      base::BindLambdaForTesting([&](const ftl::AckMessagesRequest& request) {
        EXPECT_EQ(1, request.messages_size());
        EXPECT_EQ(kFakeReceiverId, request.messages(0).receiver_id().id());
        EXPECT_EQ(kMessage1Id, request.messages(0).message_id());
        return grpc::Status::OK;
      }),
      run_loop.QuitWhenIdleClosure());

  run_loop.Run();
}

TEST_F(FtlMessagingClientTest, ReceivedDuplicatedMessage_AckAndDrop) {
  base::RunLoop run_loop;

  base::MockCallback<FtlMessagingClient::MessageCallback> mock_on_incoming_msg;
  EXPECT_CALL(mock_on_incoming_msg, Run(IsFakeSenderId(), kFakeSenderRegId,
                                        StanzaTextMatches(kMessage1Text)))
      .WillOnce(Return());

  auto subscription =
      messaging_client_->RegisterMessageCallback(mock_on_incoming_msg.Get());

  messaging_client_->PullMessages(test::CheckStatusThenQuitRunLoopCallback(
      FROM_HERE, grpc::StatusCode::OK, &run_loop));

  ftl::PullMessagesResponse pull_messages_response;
  *pull_messages_response.add_messages() =
      CreateInboxMessage(kMessage1Id, kMessage1Text);
  *pull_messages_response.add_messages() =
      CreateInboxMessage(kMessage1Id, kMessage1Text);
  ServerWaitAndRespondToPullMessagesRequest(pull_messages_response,
                                            grpc::Status::OK);

  ServerWaitAndRespondToAckMessagesRequest(
      base::BindLambdaForTesting([&](const ftl::AckMessagesRequest& request) {
        EXPECT_EQ(2, request.messages_size());
        EXPECT_EQ(kFakeReceiverId, request.messages(0).receiver_id().id());
        EXPECT_EQ(kFakeReceiverId, request.messages(1).receiver_id().id());
        EXPECT_EQ(kMessage1Id, request.messages(0).message_id());
        EXPECT_EQ(kMessage1Id, request.messages(1).message_id());
        return grpc::Status::OK;
      }),
      base::DoNothing());

  run_loop.Run();
}

}  // namespace remoting
