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
#include "remoting/base/http_status.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"
#include "remoting/signaling/ftl_client_uuid_device_id_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using testing::_;

using SignInGaiaResponseCallback =
    base::OnceCallback<void(const HttpStatus&,
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
    std::move(on_done).Run(HttpStatus::OK(), std::move(response));
  };
}

}  // namespace

class FtlRegistrationManagerTest : public testing::Test {
 protected:
  class MockRegistrationClient
      : public FtlRegistrationManager::RegistrationClient {
   public:
    MOCK_METHOD(void,
                SignInGaia,
                (const ftl::SignInGaiaRequest&, SignInGaiaResponseCallback),
                (override));
    MOCK_METHOD(void, CancelPendingRequests, (), (override));
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
  base::MockCallback<base::RepeatingCallback<void(const HttpStatus&)>>
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
  ASSERT_EQ(registration_manager_.GetRegistrationId(), "registration_id_1");
  ASSERT_EQ(registration_manager_.GetFtlAuthToken(), kAuthToken);

  task_environment_.FastForwardBy(kAuthTokenExpiration);
  task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());
  ASSERT_EQ(registration_manager_.GetRegistrationId(), "registration_id_2");
}

TEST_F(FtlRegistrationManagerTest, FailedToSignIn_Backoff) {
  ASSERT_FALSE(registration_manager_.IsSignedIn());
  ASSERT_TRUE(registration_manager_.GetRegistrationId().empty());
  ASSERT_TRUE(registration_manager_.GetFtlAuthToken().empty());
  ASSERT_EQ(GetBackoff().failure_count(), 0);

  EXPECT_CALL(*registration_client_, SignInGaia(_, _))
      .WillOnce([](const ftl::SignInGaiaRequest& request,
                   SignInGaiaResponseCallback on_done) {
        VerifySignInGaiaRequest(request);
        std::move(on_done).Run(
            HttpStatus(HttpStatus::Code::UNAVAILABLE, "unavailable"), {});
      })
      .WillOnce([](const ftl::SignInGaiaRequest& request,
                   SignInGaiaResponseCallback on_done) {
        VerifySignInGaiaRequest(request);
        std::move(on_done).Run(
            HttpStatus(HttpStatus::Code::UNAUTHENTICATED, "unauthenticated"),
            {});
      })
      .WillOnce(RespondOkToSignInGaia("registration_id"));

  EXPECT_CALL(done_callback_, Run(HasErrorCode(HttpStatus::Code::UNAVAILABLE)))
      .Times(1);
  registration_manager_.SignInGaia(done_callback_.Get());
  task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());
  ASSERT_FALSE(registration_manager_.IsSignedIn());
  ASSERT_EQ(GetBackoff().failure_count(), 1);

  EXPECT_CALL(done_callback_,
              Run(HasErrorCode(HttpStatus::Code::UNAUTHENTICATED)))
      .Times(1);
  registration_manager_.SignInGaia(done_callback_.Get());
  task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());
  ASSERT_FALSE(registration_manager_.IsSignedIn());
  ASSERT_EQ(GetBackoff().failure_count(), 2);

  EXPECT_CALL(done_callback_, Run(IsStatusOk())).Times(1);
  registration_manager_.SignInGaia(done_callback_.Get());
  task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());
  ASSERT_TRUE(registration_manager_.IsSignedIn());
  ASSERT_EQ(registration_manager_.GetRegistrationId(), "registration_id");
  ASSERT_EQ(GetBackoff().failure_count(), 0);
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
  ASSERT_EQ(registration_manager_.GetRegistrationId(), "registration_id");
  ASSERT_EQ(registration_manager_.GetFtlAuthToken(), kAuthToken);

  EXPECT_CALL(*registration_client_, CancelPendingRequests()).Times(1);

  registration_manager_.SignOut();
  ASSERT_FALSE(registration_manager_.IsSignedIn());
  ASSERT_TRUE(registration_manager_.GetRegistrationId().empty());
  ASSERT_TRUE(registration_manager_.GetFtlAuthToken().empty());

  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_FALSE(registration_manager_.IsSignedIn());
}

TEST_F(FtlRegistrationManagerTest, SignInGaia_EmptyRegistrationId) {
  EXPECT_CALL(*registration_client_, SignInGaia(_, _))
      .WillOnce([](const ftl::SignInGaiaRequest& request,
                   SignInGaiaResponseCallback on_done) {
        auto response = std::make_unique<ftl::SignInGaiaResponse>();
        response->set_registration_id("");
        response->mutable_auth_token()->set_payload(kAuthToken);
        response->mutable_auth_token()->set_expires_in(
            kAuthTokenExpiresInMicroseconds);
        std::move(on_done).Run(HttpStatus::OK(), std::move(response));
      });

  EXPECT_CALL(done_callback_, Run(HasErrorCode(HttpStatus::Code::UNKNOWN)))
      .Times(1);
  registration_manager_.SignInGaia(done_callback_.Get());
  task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());

  ASSERT_FALSE(registration_manager_.IsSignedIn());
}

TEST_F(FtlRegistrationManagerTest, SignInGaia_ShortRefreshTime) {
  EXPECT_CALL(*registration_client_, SignInGaia(_, _))
      .WillOnce([](const ftl::SignInGaiaRequest& request,
                   SignInGaiaResponseCallback on_done) {
        auto response = std::make_unique<ftl::SignInGaiaResponse>();
        response->set_registration_id("registration_id");
        response->mutable_auth_token()->set_payload(kAuthToken);
        // Set refresh time shorter than kRefreshBufferTime (1 hour).
        response->mutable_auth_token()->set_expires_in(
            base::Minutes(30).InMicroseconds());
        std::move(on_done).Run(HttpStatus::OK(), std::move(response));
      })
      .WillOnce(RespondOkToSignInGaia("registration_id_2"));

  EXPECT_CALL(done_callback_, Run(IsStatusOk())).Times(1);
  registration_manager_.SignInGaia(done_callback_.Get());
  task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());

  ASSERT_TRUE(registration_manager_.IsSignedIn());
  ASSERT_EQ(registration_manager_.GetRegistrationId(), "registration_id");

  // Should refresh in 30 minutes.
  task_environment_.FastForwardBy(base::Minutes(30));
  task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());
  ASSERT_EQ(registration_manager_.GetRegistrationId(), "registration_id_2");
}

}  // namespace remoting
