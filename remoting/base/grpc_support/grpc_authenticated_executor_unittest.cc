// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/grpc_support/grpc_authenticated_executor.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/base/grpc_support/grpc_async_unary_request.h"
#include "remoting/base/grpc_support/grpc_support_test_services.pb.h"
#include "remoting/base/oauth_token_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using testing::_;
using testing::Invoke;
using testing::Return;

class MockExecutor : public GrpcExecutor {
 public:
  MOCK_METHOD1(ExecuteRpc, void(std::unique_ptr<GrpcAsyncRequest>));
  MOCK_METHOD0(CancelPendingRequests, void());
};

class MockOAuthTokenGetter : public OAuthTokenGetter {
 public:
  MOCK_METHOD1(CallWithToken, void(TokenCallback));
  MOCK_METHOD0(InvalidateCache, void());
};

}  // namespace

class GrpcAuthenticatedExecutorTest : public testing::Test {
 public:
  void SetUp() override;

 protected:
  void SetExecutorTokenGetter(OAuthTokenGetter* token_getter);

  MockExecutor* mock_executor_;
  std::unique_ptr<GrpcAuthenticatedExecutor> executor_;

 private:
  FakeOAuthTokenGetter token_getter_{OAuthTokenGetter::Status::SUCCESS,
                                     "fake_user", "fake_token"};
  base::test::TaskEnvironment task_environment_;
};

void GrpcAuthenticatedExecutorTest::SetUp() {
  executor_ = std::make_unique<GrpcAuthenticatedExecutor>(&token_getter_);
  auto mock_executor = std::make_unique<MockExecutor>();
  mock_executor_ = mock_executor.get();
  executor_->executor_ = std::move(mock_executor);
}

void GrpcAuthenticatedExecutorTest::SetExecutorTokenGetter(
    OAuthTokenGetter* token_getter) {
  executor_->token_getter_ = token_getter;
}

// Unfortunately we can't verify whether the credentials are set because
// grpc::ClientContext has not getter for the credentials.
TEST_F(GrpcAuthenticatedExecutorTest, VerifyExecuteRpcCallIsForwarded) {
  base::RunLoop run_loop;
  auto request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce([](grpc::ClientContext*, const EchoRequest&,
                        grpc_impl::CompletionQueue*) {
        return std::unique_ptr<grpc::ClientAsyncResponseReader<EchoResponse>>();
      }),
      EchoRequest(),
      base::DoNothing::Once<const grpc::Status&, const EchoResponse&>());
  auto* request_ptr = request.get();
  EXPECT_CALL(*mock_executor_, ExecuteRpc(_))
      .WillOnce(Invoke([&](std::unique_ptr<GrpcAsyncRequest> request) {
        ASSERT_EQ(request_ptr, request.get());
        run_loop.QuitWhenIdle();
      }));
  executor_->ExecuteRpc(std::move(request));
  run_loop.Run();
}

TEST_F(GrpcAuthenticatedExecutorTest, CancelAuthenticatingRpcAndSendNewOne) {
  MockOAuthTokenGetter mock_token_getter;
  SetExecutorTokenGetter(&mock_token_getter);
  base::RunLoop run_loop_1;
  auto request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce([](grpc::ClientContext*, const EchoRequest&,
                        grpc_impl::CompletionQueue*) {
        return std::unique_ptr<grpc::ClientAsyncResponseReader<EchoResponse>>();
      }),
      EchoRequest(),
      base::DoNothing::Once<const grpc::Status&, const EchoResponse&>());

  OAuthTokenGetter::TokenCallback saved_on_access_token;
  EXPECT_CALL(mock_token_getter, CallWithToken(_))
      .WillOnce([&](OAuthTokenGetter::TokenCallback on_access_token) {
        saved_on_access_token = std::move(on_access_token);
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, run_loop_1.QuitWhenIdleClosure());
      });
  EXPECT_CALL(*mock_executor_, CancelPendingRequests()).WillOnce(Return());

  executor_->ExecuteRpc(std::move(request));
  executor_->CancelPendingRequests();
  run_loop_1.Run();

  // Nothing should happen.
  std::move(saved_on_access_token)
      .Run(OAuthTokenGetter::Status::SUCCESS, "fake_user", "fake_token");

  base::RunLoop run_loop_2;

  request = CreateGrpcAsyncUnaryRequest(
      base::BindOnce([](grpc::ClientContext*, const EchoRequest&,
                        grpc_impl::CompletionQueue*) {
        return std::unique_ptr<grpc::ClientAsyncResponseReader<EchoResponse>>();
      }),
      EchoRequest(),
      base::DoNothing::Once<const grpc::Status&, const EchoResponse&>());
  auto* request_ptr = request.get();

  EXPECT_CALL(mock_token_getter, CallWithToken(_))
      .WillOnce([&](OAuthTokenGetter::TokenCallback on_access_token) {
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(on_access_token),
                                      OAuthTokenGetter::Status::SUCCESS,
                                      "fake_user", "fake_token"));
      });

  EXPECT_CALL(*mock_executor_, ExecuteRpc(_))
      .WillOnce(Invoke([&](std::unique_ptr<GrpcAsyncRequest> request) {
        ASSERT_EQ(request_ptr, request.get());
        run_loop_2.QuitWhenIdle();
      }));
  executor_->ExecuteRpc(std::move(request));
  run_loop_2.Run();
}

}  // namespace remoting
