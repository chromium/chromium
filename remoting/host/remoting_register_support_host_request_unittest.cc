// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remoting_register_support_host_request.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/test_rsa_key_pair.h"
#include "remoting/proto/remoting/v1/remote_support_host_service.grpc.pb.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using testing::_;

using RegisterSupportHostResponseCallback =
    base::OnceCallback<void(const grpc::Status&,
                            const apis::v1::RegisterSupportHostResponse&)>;

constexpr char kSupportId[] = "123321456654";
constexpr base::TimeDelta kSupportIdLifetime = base::TimeDelta::FromMinutes(5);
constexpr char kFtlId[] = "fake_user@domain.com/chromoting_ftl_abc123";

void ValidateRegisterHost(const apis::v1::RegisterSupportHostRequest& request) {
  ASSERT_TRUE(request.has_host_version());
  ASSERT_TRUE(request.has_host_os_name());
  ASSERT_TRUE(request.has_host_os_version());
  ASSERT_EQ(kFtlId, request.tachyon_id());

  auto key_pair = RsaKeyPair::FromString(kTestRsaKeyPair);
  EXPECT_EQ(key_pair->GetPublicKey(), request.public_key());
}

void RespondOk(RegisterSupportHostResponseCallback callback) {
  apis::v1::RegisterSupportHostResponse response;
  response.set_support_id(kSupportId);
  response.set_support_id_lifetime_seconds(kSupportIdLifetime.InSeconds());
  std::move(callback).Run(grpc::Status::OK, response);
}

decltype(auto) DoValidateRegisterHostAndRespondOk() {
  return [=](const apis::v1::RegisterSupportHostRequest& request,
             RegisterSupportHostResponseCallback callback) {
    ValidateRegisterHost(request);
    RespondOk(std::move(callback));
  };
}

}  // namespace

class RemotingRegisterSupportHostTest : public testing::Test {
 public:
  RemotingRegisterSupportHostTest() {
    register_host_request_ =
        std::make_unique<RemotingRegisterSupportHostRequest>(
            std::make_unique<FakeOAuthTokenGetter>(
                OAuthTokenGetter::Status::SUCCESS, "fake_email",
                "fake_access_token"));

    auto register_host_client =
        std::make_unique<MockRegisterSupportHostClient>();
    register_host_client_ = register_host_client.get();
    register_host_request_->register_host_client_ =
        std::move(register_host_client);

    signal_strategy_ =
        std::make_unique<FakeSignalStrategy>(SignalingAddress(kFtlId));

    // Start in disconnected state.
    signal_strategy_->Disconnect();

    key_pair_ = RsaKeyPair::FromString(kTestRsaKeyPair);
  }

  ~RemotingRegisterSupportHostTest() override {
    register_host_request_.reset();
    signal_strategy_.reset();
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  class MockRegisterSupportHostClient final
      : public RemotingRegisterSupportHostRequest::RegisterSupportHostClient {
   public:
    MOCK_METHOD2(RegisterSupportHost,
                 void(const apis::v1::RegisterSupportHostRequest&,
                      RegisterSupportHostResponseCallback));
    MOCK_METHOD0(CancelPendingRequests, void());
  };

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<RemotingRegisterSupportHostRequest> register_host_request_;
  MockRegisterSupportHostClient* register_host_client_ = nullptr;

  std::unique_ptr<SignalStrategy> signal_strategy_;
  scoped_refptr<RsaKeyPair> key_pair_;
};

TEST_F(RemotingRegisterSupportHostTest, RegisterFtl) {
  EXPECT_CALL(*register_host_client_, RegisterSupportHost(_, _))
      .WillOnce(DoValidateRegisterHostAndRespondOk());
  EXPECT_CALL(*register_host_client_, CancelPendingRequests()).Times(1);

  base::MockCallback<RegisterSupportHostRequest::RegisterCallback>
      register_callback;
  EXPECT_CALL(register_callback,
              Run(kSupportId, kSupportIdLifetime, protocol::ErrorCode::OK))
      .Times(1);

  register_host_request_->StartRequest(signal_strategy_.get(), key_pair_,
                                       register_callback.Get());
  signal_strategy_->Connect();
}

TEST_F(RemotingRegisterSupportHostTest, FailedWithDeadlineExceeded) {
  EXPECT_CALL(*register_host_client_, RegisterSupportHost(_, _))
      .WillOnce([](const apis::v1::RegisterSupportHostRequest& request,
                   RegisterSupportHostResponseCallback callback) {
        ValidateRegisterHost(request);
        std::move(callback).Run(
            grpc::Status(grpc::StatusCode::DEADLINE_EXCEEDED,
                         "deadline exceeded"),
            {});
      });
  EXPECT_CALL(*register_host_client_, CancelPendingRequests()).Times(1);

  base::MockCallback<RegisterSupportHostRequest::RegisterCallback>
      register_callback;
  EXPECT_CALL(register_callback,
              Run(_, _, protocol::ErrorCode::SIGNALING_TIMEOUT))
      .Times(1);

  register_host_request_->StartRequest(signal_strategy_.get(), key_pair_,
                                       register_callback.Get());
  signal_strategy_->Connect();
}

TEST_F(RemotingRegisterSupportHostTest,
       SignalingDisconnectedBeforeRegistrationSucceeds) {
  RegisterSupportHostResponseCallback register_support_host_callback;
  EXPECT_CALL(*register_host_client_, RegisterSupportHost(_, _))
      .WillOnce([&](const apis::v1::RegisterSupportHostRequest& request,
                    RegisterSupportHostResponseCallback callback) {
        ValidateRegisterHost(request);
        register_support_host_callback = std::move(callback);
      });
  EXPECT_CALL(*register_host_client_, CancelPendingRequests()).Times(1);

  base::MockCallback<RegisterSupportHostRequest::RegisterCallback>
      register_callback;
  EXPECT_CALL(register_callback,
              Run(_, _, protocol::ErrorCode::SIGNALING_ERROR))
      .Times(1);

  register_host_request_->StartRequest(signal_strategy_.get(), key_pair_,
                                       register_callback.Get());
  signal_strategy_->Connect();
  signal_strategy_->Disconnect();
  RespondOk(std::move(register_support_host_callback));
}

}  // namespace remoting
