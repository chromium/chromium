// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/grpc_support/grpc_async_executor.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "remoting/base/grpc_support/grpc_async_server_streaming_request.h"
#include "remoting/base/grpc_support/grpc_async_unary_request.h"
#include "remoting/base/grpc_support/grpc_support_test_services.grpc.pb.h"
#include "remoting/base/grpc_support/grpc_util.h"
#include "remoting/base/grpc_test_support/grpc_async_test_server.h"
#include "remoting/base/grpc_test_support/grpc_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"

namespace remoting {

namespace {

using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::Return;

using EchoStreamResponder = test::GrpcServerStreamResponder<EchoResponse>;
using MockOnceClosure = base::MockCallback<base::OnceClosure>;
using MockMessageCallback =
    base::MockCallback<base::RepeatingCallback<void(const EchoResponse&)>>;
using MockStatusCallback =
    base::MockCallback<base::OnceCallback<void(const grpc::Status&)>>;

base::RepeatingClosure NotReachedClosure() {
  return base::BindRepeating([]() { NOTREACHED(); });
}

base::RepeatingCallback<void(const EchoResponse&)>
NotReachedStreamingCallback() {
  return base::BindRepeating([](const EchoResponse&) { NOTREACHED(); });
}

base::RepeatingCallback<void(const grpc::Status&)> NotReachedStatusCallback() {
  return base::BindRepeating([](const grpc::Status&) { NOTREACHED(); });
}

EchoResponse ResponseForText(const std::string& text) {
  EchoResponse response;
  response.set_text(text);
  return response;
}

#define EXPECT_CLOSURE_CALL_ONCE(mock_closure) \
  EXPECT_CALL(mock_closure, Run()).Times(1)

}  // namespace

class GrpcAsyncExecutorTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  void AsyncSendText(const std::string& text,
                     base::OnceCallback<void(const grpc::Status&,
                                             const EchoResponse&)> callback);

  std::unique_ptr<ScopedGrpcServerStream> StartEchoStreamOnExecutor(
      const std::string& request_text,
      base::OnceClosure on_channel_ready,
      const base::RepeatingCallback<void(const EchoResponse&)>& on_incoming_msg,
      base::OnceCallback<void(const grpc::Status&)> on_channel_closed,
      GrpcAsyncExecutor* executor,
      base::Time deadline = {});

  std::unique_ptr<ScopedGrpcServerStream> StartEchoStream(
      const std::string& request_text,
      base::OnceClosure on_channel_ready,
      const base::RepeatingCallback<void(const EchoResponse&)>& on_incoming_msg,
      base::OnceCallback<void(const grpc::Status&)> on_channel_closed);

 protected:
  void HandleOneEchoRequest();
  std::unique_ptr<test::GrpcServerResponder<EchoResponse>>
  GetResponderAndFillEchoRequest(EchoRequest* request);
  std::unique_ptr<EchoStreamResponder> HandleEchoStream(
      const base::Location& from_here,
      const std::string& expected_request_text);

  std::unique_ptr<GrpcAsyncExecutor> executor_;
  std::unique_ptr<test::GrpcAsyncTestServer> server_;
  std::unique_ptr<GrpcAsyncExecutorTestService::Stub> stub_;
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
};

void GrpcAsyncExecutorTest::SetUp() {
  task_environment_ = std::make_unique<base::test::TaskEnvironment>();
  executor_ = std::make_unique<GrpcAsyncExecutor>();
  server_ = std::make_unique<test::GrpcAsyncTestServer>(
      std::make_unique<GrpcAsyncExecutorTestService::AsyncService>());
  stub_ =
      GrpcAsyncExecutorTestService::NewStub(server_->CreateInProcessChannel());
}

void GrpcAsyncExecutorTest::TearDown() {
  server_.reset();
  executor_.reset();
  stub_.reset();
}

void GrpcAsyncExecutorTest::AsyncSendText(
    const std::string& text,
    base::OnceCallback<void(const grpc::Status&, const EchoResponse&)>
        callback) {
  EchoRequest request;
  request.set_text(text);
  auto grpc_request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&GrpcAsyncExecutorTestService::Stub::AsyncEcho,
                     base::Unretained(stub_.get())),
      request, std::move(callback));
  executor_->ExecuteRpc(std::move(grpc_request));
}

std::unique_ptr<ScopedGrpcServerStream>
GrpcAsyncExecutorTest::StartEchoStreamOnExecutor(
    const std::string& request_text,
    base::OnceClosure on_channel_ready,
    const base::RepeatingCallback<void(const EchoResponse&)>& on_incoming_msg,
    base::OnceCallback<void(const grpc::Status&)> on_channel_closed,
    GrpcAsyncExecutor* executor,
    base::Time deadline) {
  EchoRequest request;
  request.set_text(request_text);
  std::unique_ptr<ScopedGrpcServerStream> scoped_stream;
  auto grpc_request = CreateGrpcAsyncServerStreamingRequest(
      base::BindOnce(&GrpcAsyncExecutorTestService::Stub::AsyncStreamEcho,
                     base::Unretained(stub_.get())),
      request, std::move(on_channel_ready), on_incoming_msg,
      std::move(on_channel_closed), &scoped_stream);
  if (!deadline.is_null()) {
    grpc_request->set_initial_metadata_deadline(deadline);
  }
  executor->ExecuteRpc(std::move(grpc_request));
  return scoped_stream;
}

std::unique_ptr<ScopedGrpcServerStream> GrpcAsyncExecutorTest::StartEchoStream(
    const std::string& request_text,
    base::OnceClosure on_channel_ready,
    const base::RepeatingCallback<void(const EchoResponse&)>& on_incoming_msg,
    base::OnceCallback<void(const grpc::Status&)> on_channel_closed) {
  return StartEchoStreamOnExecutor(
      request_text, std::move(on_channel_ready), on_incoming_msg,
      std::move(on_channel_closed), executor_.get());
}

void GrpcAsyncExecutorTest::HandleOneEchoRequest() {
  EchoRequest request;
  auto responder = GetResponderAndFillEchoRequest(&request);
  EchoResponse response;
  response.set_text(request.text());
  ASSERT_TRUE(responder->Respond(response, grpc::Status::OK));
}

std::unique_ptr<test::GrpcServerResponder<EchoResponse>>
GrpcAsyncExecutorTest::GetResponderAndFillEchoRequest(EchoRequest* request) {
  return server_->HandleRequest(
      &GrpcAsyncExecutorTestService::AsyncService::RequestEcho, request);
}

std::unique_ptr<EchoStreamResponder> GrpcAsyncExecutorTest::HandleEchoStream(
    const base::Location& from_here,
    const std::string& expected_request_text) {
  EchoRequest request;
  auto responder = server_->HandleStreamRequest(
      &GrpcAsyncExecutorTestService::AsyncService::RequestStreamEcho, &request);
  EXPECT_EQ(expected_request_text, request.text())
      << "Request text mismatched. Location: " << from_here.ToString();
  return responder;
}

TEST_F(GrpcAsyncExecutorTest, DoNothing) {}

TEST_F(GrpcAsyncExecutorTest, SendOneTextAndRespond) {
  base::RunLoop run_loop;
  AsyncSendText("Hello",
                base::BindLambdaForTesting([&](const grpc::Status& status,
                                               const EchoResponse& response) {
                  EXPECT_TRUE(status.ok());
                  EXPECT_EQ("Hello", response.text());
                  run_loop.QuitWhenIdle();
                }));
  HandleOneEchoRequest();
  run_loop.Run();
}

TEST_F(GrpcAsyncExecutorTest, SendTwoTextsAndRespondOneByOne) {
  base::RunLoop run_loop_1;
  AsyncSendText("Hello 1",
                base::BindLambdaForTesting([&](const grpc::Status& status,
                                               const EchoResponse& response) {
                  EXPECT_TRUE(status.ok());
                  EXPECT_EQ("Hello 1", response.text());
                  run_loop_1.QuitWhenIdle();
                }));
  HandleOneEchoRequest();
  run_loop_1.Run();

  base::RunLoop run_loop_2;
  AsyncSendText("Hello 2",
                base::BindLambdaForTesting([&](const grpc::Status& status,
                                               const EchoResponse& response) {
                  EXPECT_TRUE(status.ok());
                  EXPECT_EQ("Hello 2", response.text());
                  run_loop_2.QuitWhenIdle();
                }));
  HandleOneEchoRequest();
  run_loop_2.Run();
}

TEST_F(GrpcAsyncExecutorTest, SendTwoTextsAndRespondTogether) {
  base::RunLoop run_loop;
  size_t response_count = 0;
  auto on_received_one_response = [&]() {
    response_count++;
    if (response_count == 2) {
      run_loop.QuitWhenIdle();
    }
  };
  AsyncSendText("Hello 1",
                base::BindLambdaForTesting([&](const grpc::Status& status,
                                               const EchoResponse& response) {
                  EXPECT_TRUE(status.ok());
                  EXPECT_EQ("Hello 1", response.text());
                  on_received_one_response();
                }));
  AsyncSendText("Hello 2",
                base::BindLambdaForTesting([&](const grpc::Status& status,
                                               const EchoResponse& response) {
                  EXPECT_TRUE(status.ok());
                  EXPECT_EQ("Hello 2", response.text());
                  on_received_one_response();
                }));
  HandleOneEchoRequest();
  HandleOneEchoRequest();
  run_loop.Run();
}

TEST_F(GrpcAsyncExecutorTest,
       ControlGroup_RpcChannelStillOpenAfterRunLoopQuit) {
  base::RunLoop run_loop;
  AsyncSendText("Hello", base::BindLambdaForTesting(
                             [&](const grpc::Status&, const EchoResponse&) {
                               NOTREACHED();
                             }));
  EchoRequest request;
  auto responder = GetResponderAndFillEchoRequest(&request);
  run_loop.RunUntilIdle();
  ASSERT_TRUE(responder->Respond(EchoResponse(), grpc::Status::OK));
}

TEST_F(GrpcAsyncExecutorTest, RpcCanceledOnDestruction) {
  base::RunLoop run_loop;
  AsyncSendText("Hello", base::BindLambdaForTesting(
                             [&](const grpc::Status&, const EchoResponse&) {
                               NOTREACHED();
                             }));
  EchoRequest request;
  auto responder = GetResponderAndFillEchoRequest(&request);
  executor_.reset();
  run_loop.RunUntilIdle();
  ASSERT_FALSE(responder->Respond(EchoResponse(), grpc::Status::OK));
}

TEST_F(GrpcAsyncExecutorTest, UnaryRpcCanceledBeforeExecution) {
  EchoRequest request;
  request.set_text("Hello 1");
  auto grpc_request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&GrpcAsyncExecutorTestService::Stub::AsyncEcho,
                     base::Unretained(stub_.get())),
      request, base::BindOnce([](const grpc::Status&, const EchoResponse&) {
        NOTREACHED();
      }));
  grpc_request->CancelRequest();
  executor_->ExecuteRpc(std::move(grpc_request));

  AsyncSendText("Hello 2", base::BindOnce([](const grpc::Status& status,
                                             const EchoResponse& response) {
                  NOTREACHED();
                }));

  // Verify that the second request is received instead of the first one.
  EchoRequest received_request;
  auto responder = GetResponderAndFillEchoRequest(&received_request);
  ASSERT_EQ("Hello 2", received_request.text());
}

TEST_F(GrpcAsyncExecutorTest, CancelAllRpcsAndExecuteANewOne) {
  base::RunLoop run_loop_1;
  AsyncSendText("Hello 1", base::BindLambdaForTesting(
                               [&](const grpc::Status&, const EchoResponse&) {
                                 NOTREACHED();
                               }));
  executor_->CancelPendingRequests();
  EchoRequest received_request;
  auto responder = GetResponderAndFillEchoRequest(&received_request);
  ASSERT_EQ("Hello 1", received_request.text());
  // Response fails to deliver.
  ASSERT_FALSE(responder->Respond(EchoResponse(), grpc::Status::OK));
  run_loop_1.RunUntilIdle();
  base::RunLoop run_loop_2;
  AsyncSendText("Hello 2",
                base::BindLambdaForTesting([&](const grpc::Status& status,
                                               const EchoResponse& response) {
                  ASSERT_TRUE(status.ok());
                  ASSERT_EQ("Hello 2", response.text());
                  run_loop_2.Quit();
                }));
  HandleOneEchoRequest();
  run_loop_2.Run();
}

TEST_F(GrpcAsyncExecutorTest, ServerStreamingRpcCanceledBeforeExecution) {
  EchoRequest request;
  request.set_text("Hello 1");
  std::unique_ptr<ScopedGrpcServerStream> scoped_stream_1;
  auto grpc_request = CreateGrpcAsyncServerStreamingRequest(
      base::BindOnce(&GrpcAsyncExecutorTestService::Stub::AsyncStreamEcho,
                     base::Unretained(stub_.get())),
      request, NotReachedClosure(), NotReachedStreamingCallback(),
      NotReachedStatusCallback(), &scoped_stream_1);
  scoped_stream_1.reset();
  executor_->ExecuteRpc(std::move(grpc_request));

  auto scoped_stream_2 = StartEchoStream("Hello 2", NotReachedClosure(),
                                         NotReachedStreamingCallback(),
                                         NotReachedStatusCallback());

  // Verify that the second request is received instead of the first one.
  EchoRequest received_request;
  auto responder = server_->HandleStreamRequest(
      &GrpcAsyncExecutorTestService::AsyncService::RequestStreamEcho,
      &received_request);
  ASSERT_EQ("Hello 2", received_request.text());
}

TEST_F(GrpcAsyncExecutorTest, ServerStreamNotAcceptedByServer) {
  base::RunLoop run_loop;
  auto scoped_stream = StartEchoStream("Hello", NotReachedClosure(),
                                       NotReachedStreamingCallback(),
                                       NotReachedStatusCallback());
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        executor_.reset();
        run_loop.QuitWhenIdle();
      }));
  run_loop.Run();
}

TEST_F(GrpcAsyncExecutorTest, ServerStreamImmediatelyClosedByServer) {
  base::RunLoop run_loop;
  MockOnceClosure on_channel_ready;
  EXPECT_CLOSURE_CALL_ONCE(on_channel_ready);
  auto scoped_stream = StartEchoStream(
      "Hello", on_channel_ready.Get(), NotReachedStreamingCallback(),
      test::CheckStatusThenQuitRunLoopCallback(FROM_HERE, grpc::StatusCode::OK,
                                               &run_loop));
  auto responder = HandleEchoStream(FROM_HERE, "Hello");
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() { responder.reset(); }));
  run_loop.Run();
}

TEST_F(GrpcAsyncExecutorTest, ServerStreamImmediatelyClosedByServerWithError) {
  base::RunLoop run_loop;
  MockOnceClosure on_channel_ready;
  EXPECT_CLOSURE_CALL_ONCE(on_channel_ready);
  auto scoped_stream = StartEchoStream(
      "Hello", on_channel_ready.Get(), NotReachedStreamingCallback(),
      test::CheckStatusThenQuitRunLoopCallback(
          FROM_HERE, grpc::StatusCode::UNAUTHENTICATED, &run_loop));
  auto responder = HandleEchoStream(FROM_HERE, "Hello");
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        responder->Close(grpc::Status(grpc::StatusCode::UNAUTHENTICATED, ""));
      }));
  run_loop.Run();
}

TEST_F(GrpcAsyncExecutorTest, ServerStreamsOneMessageThenClosedByServer) {
  base::RunLoop run_loop;
  MockOnceClosure on_channel_ready;
  EXPECT_CLOSURE_CALL_ONCE(on_channel_ready);
  std::unique_ptr<EchoStreamResponder> responder;
  auto scoped_stream = StartEchoStream(
      "Hello", on_channel_ready.Get(),
      base::BindLambdaForTesting([&](const EchoResponse& response) {
        ASSERT_EQ("Echo 1", response.text());
        responder->WaitForSendMessageResult();
        responder.reset();
      }),
      test::CheckStatusThenQuitRunLoopCallback(FROM_HERE, grpc::StatusCode::OK,
                                               &run_loop));
  responder = HandleEchoStream(FROM_HERE, "Hello");
  responder->SendMessage(ResponseForText("Echo 1"));
  run_loop.Run();
}

TEST_F(GrpcAsyncExecutorTest, ServerStreamsTwoMessagesThenClosedByServer) {
  base::RunLoop run_loop;
  std::unique_ptr<EchoStreamResponder> responder;

  MockOnceClosure on_channel_ready;
  EXPECT_CLOSURE_CALL_ONCE(on_channel_ready);

  base::MockCallback<base::RepeatingCallback<void(const EchoResponse&)>>
      mock_on_incoming_msg;

  {
    InSequence sequence;
    EXPECT_CALL(mock_on_incoming_msg,
                Run(Property(&EchoResponse::text, "Echo 1")))
        .WillOnce(Invoke([&](const EchoResponse&) {
          ASSERT_TRUE(responder->WaitForSendMessageResult());
          responder->SendMessage(ResponseForText("Echo 2"));
        }));
    EXPECT_CALL(mock_on_incoming_msg,
                Run(Property(&EchoResponse::text, "Echo 2")))
        .WillOnce(Invoke([&](const EchoResponse&) {
          ASSERT_TRUE(responder->WaitForSendMessageResult());
          responder.reset();
        }));
  }

  auto scoped_stream = StartEchoStream(
      "Hello", on_channel_ready.Get(), mock_on_incoming_msg.Get(),
      test::CheckStatusThenQuitRunLoopCallback(FROM_HERE, grpc::StatusCode::OK,
                                               &run_loop));
  responder = HandleEchoStream(FROM_HERE, "Hello");
  responder->SendMessage(ResponseForText("Echo 1"));
  run_loop.Run();
}

TEST_F(GrpcAsyncExecutorTest, ServerStreamOpenThenClosedByClientAtDestruction) {
  base::RunLoop run_loop;
  auto scoped_stream = StartEchoStream("Hello", NotReachedClosure(),
                                       NotReachedStreamingCallback(),
                                       NotReachedStatusCallback());
  auto responder = HandleEchoStream(FROM_HERE, "Hello");
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        executor_.reset();
        run_loop.QuitWhenIdle();
      }));
  run_loop.Run();
  responder->SendMessage(ResponseForText("Echo 1"));
  ASSERT_FALSE(responder->WaitForSendMessageResult());
}

TEST_F(GrpcAsyncExecutorTest, ServerStreamClosedByStreamHolder) {
  base::RunLoop run_loop;
  auto scoped_stream = StartEchoStream("Hello", NotReachedClosure(),
                                       NotReachedStreamingCallback(),
                                       NotReachedStatusCallback());
  auto responder = HandleEchoStream(FROM_HERE, "Hello");
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        scoped_stream.reset();
        run_loop.QuitWhenIdle();
      }));
  run_loop.Run();
  responder->SendMessage(ResponseForText("Echo 1"));
  ASSERT_FALSE(responder->WaitForSendMessageResult());
}

TEST_F(GrpcAsyncExecutorTest, ServerStreamsOneMessageThenClosedByStreamHolder) {
  base::RunLoop run_loop;
  std::unique_ptr<EchoStreamResponder> responder;

  MockOnceClosure on_channel_ready;
  EXPECT_CLOSURE_CALL_ONCE(on_channel_ready);

  std::unique_ptr<ScopedGrpcServerStream> scoped_stream = StartEchoStream(
      "Hello", on_channel_ready.Get(),
      base::BindLambdaForTesting([&](const EchoResponse& response) {
        ASSERT_EQ("Echo 1", response.text());
        ASSERT_TRUE(responder->WaitForSendMessageResult());
        scoped_stream.reset();
        run_loop.QuitWhenIdle();
      }),
      NotReachedStatusCallback());
  responder = HandleEchoStream(FROM_HERE, "Hello");
  responder->SendMessage(ResponseForText("Echo 1"));
  run_loop.Run();
  responder->SendMessage(ResponseForText("Echo 2"));
  ASSERT_FALSE(responder->WaitForSendMessageResult());
}

TEST_F(GrpcAsyncExecutorTest, StreamWithTwoExecutors_VerifyNoInterference) {
  GrpcAsyncExecutor executor_1;
  GrpcAsyncExecutor executor_2;

  base::RunLoop run_loop;

  std::unique_ptr<EchoStreamResponder> responder_1;
  std::unique_ptr<EchoStreamResponder> responder_2;

  // Message receive order: 1-1 => 2-1 => 1-2 => 2-2
  // executor_1 receives 1-1, 1-2; executor_2 receives 2-1, 2-2
  base::MockCallback<base::RepeatingCallback<void(const EchoResponse&)>>
      mock_on_incoming_msg_1;
  {
    InSequence sequence;
    EXPECT_CALL(mock_on_incoming_msg_1,
                Run(Property(&EchoResponse::text, "1-1")))
        .WillOnce([&](const EchoResponse&) {
          ASSERT_TRUE(responder_1->WaitForSendMessageResult());
          responder_2->SendMessage(ResponseForText("2-1"));
        });
    EXPECT_CALL(mock_on_incoming_msg_1,
                Run(Property(&EchoResponse::text, "1-2")))
        .WillOnce([&](const EchoResponse&) {
          ASSERT_TRUE(responder_1->WaitForSendMessageResult());
          responder_1.reset();
          responder_2->SendMessage(ResponseForText("2-2"));
        });
  }

  base::MockCallback<base::RepeatingCallback<void(const EchoResponse&)>>
      mock_on_incoming_msg_2;
  {
    InSequence sequence;
    EXPECT_CALL(mock_on_incoming_msg_2,
                Run(Property(&EchoResponse::text, "2-1")))
        .WillOnce([&](const EchoResponse&) {
          ASSERT_TRUE(responder_2->WaitForSendMessageResult());
          responder_1->SendMessage(ResponseForText("1-2"));
        });
    EXPECT_CALL(mock_on_incoming_msg_2,
                Run(Property(&EchoResponse::text, "2-2")))
        .WillOnce([&](const EchoResponse&) {
          ASSERT_TRUE(responder_2->WaitForSendMessageResult());
          responder_2.reset();
        });
  }

  MockOnceClosure on_channel_ready_1;
  EXPECT_CLOSURE_CALL_ONCE(on_channel_ready_1);

  MockOnceClosure on_channel_ready_2;
  EXPECT_CLOSURE_CALL_ONCE(on_channel_ready_2);

  base::MockCallback<base::RepeatingCallback<void(const grpc::Status&)>>
      mock_status_callback;
  EXPECT_CALL(mock_status_callback,
              Run(Property(&grpc::Status::error_code, grpc::StatusCode::OK)))
      .WillOnce(Return())
      .WillOnce([&](const grpc::Status&) { run_loop.QuitWhenIdle(); });

  auto scoped_stream_1 = StartEchoStreamOnExecutor(
      "Hello 1", on_channel_ready_1.Get(), mock_on_incoming_msg_1.Get(),
      mock_status_callback.Get(), &executor_1);
  EchoRequest request_1;
  responder_1 = server_->HandleStreamRequest(
      &GrpcAsyncExecutorTestService::AsyncService::RequestStreamEcho,
      &request_1);

  auto scoped_stream_2 = StartEchoStreamOnExecutor(
      "Hello 2", on_channel_ready_2.Get(), mock_on_incoming_msg_2.Get(),
      mock_status_callback.Get(), &executor_2);
  EchoRequest request_2;
  responder_2 = server_->HandleStreamRequest(
      &GrpcAsyncExecutorTestService::AsyncService::RequestStreamEcho,
      &request_2);

  responder_1->SendMessage(ResponseForText("1-1"));

  run_loop.Run();
}

TEST_F(GrpcAsyncExecutorTest, ExecuteWithoutDeadline_DefaultDeadlineSet) {
  EchoRequest request;
  auto grpc_request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&GrpcAsyncExecutorTestService::Stub::AsyncEcho,
                     base::Unretained(stub_.get())),
      request, base::BindOnce([](const grpc::Status&, const EchoResponse&) {
        NOTREACHED();
      }));
  auto* context = grpc_request->context();
  ASSERT_TRUE(GetDeadline(*context).is_max());
  base::Time min_deadline = base::Time::Now();
  base::Time max_deadline = min_deadline + base::TimeDelta::FromHours(1);
  executor_->ExecuteRpc(std::move(grpc_request));
  base::Time deadline = GetDeadline(*context);
  ASSERT_LT(min_deadline, deadline);
  ASSERT_GT(max_deadline, deadline);
}

TEST_F(GrpcAsyncExecutorTest, ExecuteWithDeadline_DeadlineNotChanged) {
  constexpr base::TimeDelta kDeadlineEpsilon = base::TimeDelta::FromSeconds(1);
  EchoRequest request;
  auto grpc_request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce(&GrpcAsyncExecutorTestService::Stub::AsyncEcho,
                     base::Unretained(stub_.get())),
      request, base::BindOnce([](const grpc::Status&, const EchoResponse&) {
        NOTREACHED();
      }));
  base::Time deadline = base::Time::Now() + base::TimeDelta::FromSeconds(10);
  SetDeadline(grpc_request->context(), deadline);
  auto* unowned_context = grpc_request->context();
  executor_->ExecuteRpc(std::move(grpc_request));
  base::Time new_deadline = GetDeadline(*unowned_context);
  ASSERT_LT(deadline - kDeadlineEpsilon, new_deadline);
  ASSERT_GT(deadline + kDeadlineEpsilon, new_deadline);
}

TEST_F(GrpcAsyncExecutorTest,
       ServerStreamInitialMetadataDeadline_DefaultDeadline) {
  // Other tests can't work with mock time so we have to replace it here.
  // We also have to reset the old task environment before creating a new one
  // since only one TaskEnvironment can exist at a time.
  task_environment_.reset();
  task_environment_ = std::make_unique<base::test::TaskEnvironment>(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  MockOnceClosure on_channel_ready;
  MockMessageCallback on_incoming_message;
  MockStatusCallback on_status;
  auto scoped_stream =
      StartEchoStream("Hello", on_channel_ready.Get(),
                      on_incoming_message.Get(), on_status.Get());
  EXPECT_CALL(on_status, Run(Property(&grpc::Status::error_code,
                                      grpc::StatusCode::DEADLINE_EXCEEDED)));
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(30));
}

TEST_F(GrpcAsyncExecutorTest,
       ServerStreamInitialMetadataDeadline_ManualDeadline) {
  task_environment_.reset();
  task_environment_ = std::make_unique<base::test::TaskEnvironment>(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  MockOnceClosure on_channel_ready;
  MockMessageCallback on_incoming_message;
  MockStatusCallback on_status;
  auto scoped_stream = StartEchoStreamOnExecutor(
      "Hello", on_channel_ready.Get(), on_incoming_message.Get(),
      on_status.Get(), executor_.get(),
      base::Time::Now() + base::TimeDelta::FromSeconds(60));

  // |on_status| shouldn't be called in the first 30 seconds.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(30));

  EXPECT_CALL(on_status, Run(Property(&grpc::Status::error_code,
                                      grpc::StatusCode::DEADLINE_EXCEEDED)));

  // |on_status| should be called here.
  task_environment_->FastForwardBy(base::TimeDelta::FromSeconds(30));
}

}  // namespace remoting
