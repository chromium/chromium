// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_registration_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"
#include "remoting/signaling/ftl_client_uuid_device_id_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using testing::_;

using SignInGaiaResponseCallback =
    base::OnceCallback<void(const ProtobufHttpStatus&,
                            std::unique_ptr<ftl::SignInGaiaResponse>)>;

constexpr char kAuthToken[] = "auth_token";
constexpr int64_t kAuthTokenExpiresInMicroseconds = 86400000000;  // = 1 day
constexpr base::TimeDelta kAuthTokenExpiration =
    base::Microseconds(kAuthTokenExpiresInMicroseconds);

MATCHER_P(HasErrorCode, error_code, "") {
  return arg.error_code() == error_code;
}

MATCHER(IsStatusOk, "") {
  return arg.ok();
}

void VerifySignInGaiaRequest(const ftl::SignInGaiaRequest& request) {
  ASSERT_EQ(ftl::SignInGaiaMode_Value_DEFAULT_CREATE_ACCOUNT, request.mode());
  ASSERT_TRUE(
      base::Uuid::ParseCaseInsensitive(request.register_data().device_id().id())
          .is_valid());
  ASSERT_LT(0, request.register_data().caps_size());
}

decltype(auto) RespondOkToSignInGaia(const std::string& registration_id) {
  return [registration_id](const ftl::SignInGaiaRequest& request,
                           SignInGaiaResponseCallback on_done) {
    VerifySignInGaiaRequest(request);
    auto response = std::make_unique<ftl::SignInGaiaResponse>();
    response->set_registration_id(registration_id);
    response->mutable_auth_token()->set_payload(kAuthToken);
    response->mutable_auth_token()->set_expires_in(
        kAuthTokenExpiresInMicroseconds);
    std::move(on_done).Run(ProtobufHttpStatus::OK(), std::move(response));
  };
}

}  // namespace

class FtlRegistrationManagerTest : public testing::Test {
 protected:
  class MockRegistrationClient
      : public FtlRegistrationManager::RegistrationClient {
   public:
    MOCK_METHOD2(SignInGaia,
                 void(const ftl::SignInGaiaRequest&,
                      SignInGaiaResponseCallback));
    MOCK_METHOD0(CancelPendingRequests, void());
  };

  const net::BackoffEntry& GetBackoff() const {
    return registration_manager_.sign_in_backoff_;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FtlRegistrationManager registration_manager_{
      std::make_unique<MockRegistrationClient>(),
      std::make_unique<FtlClientUuidDeviceIdProvider>()};
  raw_ptr<MockRegistrationClient> registration_client_ =
      static_cast<MockRegistrationClient*>(
          registration_manager_.registration_client_.get());
  base::MockCallback<base::RepeatingCallback<void(const ProtobufHttpStatus&)>>
      done_callback_;
};

TEST_F(FtlRegistrationManagerTest, SignInGaiaAndAutorefresh) {
  ASSERT_FALSE(registration_manager_.IsSignedIn());
  ASSERT_TRUE(registration_manager_.GetRegistrationId().empty());
  ASSERT_TRUE(registration_manager_.GetFtlAuthToken().empty());

  EXPECT_CALL(*registration_client_, SignInGaia(_, _))
      .WillOnce(RespondOkToSignInGaia("registration_id_1"))
      .WillOnce(RespondOkToSignInGaia("registration_id_2"));

  EXPECT_CALL(done_callback_, Run(IsStatusOk())).Times(1);
  registration_manager_.SignInGaia(done_callback_.Get());
  task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());

  ASSERT_TRUE(registration_manager_.IsSignedIn());
  ASSERT_EQ("registration_id_1", registration_manager_.GetRegistrationId());
  ASSERT_EQ(kAuthToken, registration_manager_.GetFtlAuthToken());

  task_environment_.FastForwardBy(kAuthTokenExpiration);
  task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());
  ASSERT_EQ("registration_id_2", registration_manager_.GetRegistrationId());
}

TEST_F(FtlRegistrationManagerTest, FailedToSignIn_Backoff) {
  ASSERT_FALSE(registration_manager_.IsSignedIn());
  ASSERT_TRUE(registration_manager_.GetRegistrationId().empty());
  ASSERT_TRUE(registration_manager_.GetFtlAuthToken().empty());
  ASSERT_EQ(0, GetBackoff().failure_count());

  EXPECT_CALL(*registration_client_, SignInGaia(_, _))
      .WillOnce([](const ftl::SignInGaiaRequest& request,
                   SignInGaiaResponseCallback on_done) {
        VerifySignInGaiaRequest(request);
        std::move(on_done).Run(
            ProtobufHttpStatus(ProtobufHttpStatus::Code::UNAVAILABLE,
                               "unavailable"),
            {});
      })
      .WillOnce([](const ftl::SignInGaiaRequest& request,
                   SignInGaiaResponseCallback on_done) {
        VerifySignInGaiaRequest(request);
        std::move(on_done).Run(
            ProtobufHttpStatus(ProtobufHttpStatus::Code::UNAUTHENTICATED,
                               "unauthenticated"),
            {});
      })
      .WillOnce(RespondOkToSignInGaia("registration_id"));

  EXPECT_CALL(done_callback_,
              Run(HasErrorCode(ProtobufHttpStatus::Code::UNAVAILABLE)))
      .Times(1);
  registration_manager_.SignInGaia(done_callback_.Get());
  task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());
  ASSERT_FALSE(registration_manager_.IsSignedIn());
  ASSERT_EQ(1, GetBackoff().failure_count());

  EXPECT_CALL(done_callback_,
              Run(HasErrorCode(ProtobufHttpStatus::Code::UNAUTHENTICATED)))
      .Times(1);
  registration_manager_.SignInGaia(done_callback_.Get());
  task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());
  ASSERT_FALSE(registration_manager_.IsSignedIn());
  ASSERT_EQ(2, GetBackoff().failure_count());

  EXPECT_CALL(done_callback_, Run(IsStatusOk())).Times(1);
  registration_manager_.SignInGaia(done_callback_.Get());
  task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());
  ASSERT_TRUE(registration_manager_.IsSignedIn());
  ASSERT_EQ("registration_id", registration_manager_.GetRegistrationId());
  ASSERT_EQ(0, GetBackoff().failure_count());
}

TEST_F(FtlRegistrationManagerTest, SignOut) {
  ASSERT_FALSE(registration_manager_.IsSignedIn());
  ASSERT_TRUE(registration_manager_.GetRegistrationId().empty());
  ASSERT_TRUE(registration_manager_.GetFtlAuthToken().empty());

  EXPECT_CALL(*registration_client_, SignInGaia(_, _))
      .WillOnce(RespondOkToSignInGaia("registration_id"));

  EXPECT_CALL(done_callback_, Run(IsStatusOk())).Times(1);
  registration_manager_.SignInGaia(done_callback_.Get());
  task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());

  ASSERT_TRUE(registration_manager_.IsSignedIn());
  ASSERT_EQ("registration_id", registration_manager_.GetRegistrationId());
  ASSERT_EQ(kAuthToken, registration_manager_.GetFtlAuthToken());

  EXPECT_CALL(*registration_client_, CancelPendingRequests()).Times(1);

  registration_manager_.SignOut();
  ASSERT_FALSE(registration_manager_.IsSignedIn());
  ASSERT_TRUE(registration_manager_.GetRegistrationId().empty());
  ASSERT_TRUE(registration_manager_.GetFtlAuthToken().empty());

  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_FALSE(registration_manager_.IsSignedIn());
}

}  // namespace remoting
