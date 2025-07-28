// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/session_authz_reauthorizer.h"

#include <memory>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/backoff_entry.h"
#include "net/http/http_status_code.h"
#include "remoting/base/http_status.h"
#include "remoting/base/mock_session_authz_service_client.h"
#include "remoting/proto/session_authz_service.h"
#include "remoting/protocol/errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::protocol {
namespace {

using testing::_;

constexpr std::string_view kSessionId = "fake_session_id";
constexpr std::string_view kInitialReauthToken = "fake_initial_reauth_token";
constexpr base::TimeDelta kInitialTokenLifetime = base::Minutes(10);

auto Respond(std::string_view session_reauth_token,
             base::TimeDelta session_reauth_token_lifetime) {
  auto response = std::make_unique<internal::ReauthorizeHostResponseStruct>();
  response->session_reauth_token = session_reauth_token;
  response->session_reauth_token_lifetime = session_reauth_token_lifetime;
  return base::test::RunOnceCallback<3>(HttpStatus::OK(), std::move(response));
}

template <typename CodeType>
auto RespondError(CodeType code) {
  return base::test::RunOnceCallbackRepeatedly<3>(HttpStatus(code), nullptr);
}

}  // namespace

class SessionAuthzReauthorizerTest : public testing::Test {
 public:
  SessionAuthzReauthorizerTest();
  ~SessionAuthzReauthorizerTest() override;

 protected:
  auto ResetReauthorizer();

  void InitializeReauthorizer(base::TimeDelta session_reauth_token_lifetime);

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::MockCallback<SessionAuthzReauthorizer::OnReauthorizationFailedCallback>
      on_reauthorization_failed_callback_;
  std::unique_ptr<SessionAuthzReauthorizer> reauthorizer_;
  MockSessionAuthzServiceClient service_client_;
  base::TimeTicks initial_token_expire_time_;
};

SessionAuthzReauthorizerTest::SessionAuthzReauthorizerTest() {
  InitializeReauthorizer(kInitialTokenLifetime);
}

SessionAuthzReauthorizerTest::~SessionAuthzReauthorizerTest() = default;

auto SessionAuthzReauthorizerTest::ResetReauthorizer() {
  return [&]() { reauthorizer_.reset(); };
}

void SessionAuthzReauthorizerTest::InitializeReauthorizer(
    base::TimeDelta session_reauth_token_lifetime) {
  reauthorizer_ = std::make_unique<SessionAuthzReauthorizer>(
      &service_client_, kSessionId, kInitialReauthToken,
      session_reauth_token_lifetime, on_reauthorization_failed_callback_.Get());
  initial_token_expire_time_ =
      base::TimeTicks::Now() + session_reauth_token_lifetime;
  reauthorizer_->Start();
}

TEST_F(SessionAuthzReauthorizerTest, MultipleSuccessfulReauths) {
  // Reauth is not triggered before half of the token lifetime has passed for a
  // token with a lifetime of 10 minutes.
  EXPECT_CALL(service_client_, ReauthorizeHost(_, _, _, _)).Times(0);
  task_environment_.FastForwardBy(kInitialTokenLifetime / 2 -
                                  base::Seconds(10));

  // Reauth is triggered now.
  EXPECT_CALL(service_client_, ReauthorizeHost(kInitialReauthToken, kSessionId,
                                               initial_token_expire_time_, _))
      .WillOnce(Respond("fake_second_reauth_token", base::Minutes(8)));
  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_CALL(service_client_, ReauthorizeHost(_, _, _, _)).Times(0);
  task_environment_.FastForwardBy(base::Minutes(4) - base::Seconds(10));

  EXPECT_CALL(service_client_,
              ReauthorizeHost("fake_second_reauth_token", kSessionId, _, _))
      .WillOnce(Respond("fake_third_reauth_token", base::Minutes(6)));
  task_environment_.FastForwardBy(base::Seconds(10));
}

TEST_F(SessionAuthzReauthorizerTest,
       ReauthFailedWithNonretriableError_ClosesSession) {
  EXPECT_CALL(service_client_, ReauthorizeHost(kInitialReauthToken, kSessionId,
                                               initial_token_expire_time_, _))
      .WillOnce(RespondError(net::HTTP_FORBIDDEN));
  EXPECT_CALL(on_reauthorization_failed_callback_,
              Run(HttpStatus::Code::PERMISSION_DENIED, _))
      .WillOnce(ResetReauthorizer());
  task_environment_.FastForwardBy(kInitialTokenLifetime / 2);
}

TEST_F(SessionAuthzReauthorizerTest, MaxReauthRetryDurationApplied) {
  // Initialize the Reauthorizer with a token lifetime of 1 hour.
  InitializeReauthorizer(base::Minutes(60));

  // Reauth is not triggered before the maximum reauth refresh duration.
  EXPECT_CALL(service_client_, ReauthorizeHost(_, _, _, _)).Times(0);
  task_environment_.FastForwardBy(base::Minutes(50) - base::Seconds(10));

  // Reauth is triggered and includes a 30 minute token.
  EXPECT_CALL(service_client_, ReauthorizeHost(kInitialReauthToken, kSessionId,
                                               initial_token_expire_time_, _))
      .WillOnce(Respond("fake_second_reauth_token", base::Minutes(30)));
  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_CALL(service_client_, ReauthorizeHost(_, _, _, _)).Times(0);
  task_environment_.FastForwardBy(base::Minutes(20) - base::Seconds(10));

  EXPECT_CALL(service_client_,
              ReauthorizeHost("fake_second_reauth_token", kSessionId, _, _))
      .WillOnce(Respond("fake_third_reauth_token", base::Minutes(60)));
  task_environment_.FastForwardBy(base::Seconds(10));
}

TEST_F(SessionAuthzReauthorizerTest, MinReauthRetryDurationApplied) {
  // Initialize the Reauthorizer with a token lifetime of 4 minutes (below the
  // minimum of 5 minutes).
  InitializeReauthorizer(base::Minutes(4));

  // Minimum expire time is actually 2 1/2 minutes.
  auto actual_token_expire_time = base::Minutes(2) + base::Seconds(30);

  // Reauth is not triggered after 1/2 of the token lifetime and just before the
  // minimum reauth refresh duration.
  EXPECT_CALL(service_client_, ReauthorizeHost(_, _, _, _)).Times(0);
  task_environment_.FastForwardBy(actual_token_expire_time - base::Seconds(10));

  // Reauth is triggered and the second response includes a 30 minute token.
  EXPECT_CALL(service_client_,
              ReauthorizeHost(kInitialReauthToken, kSessionId, _, _))
      .WillOnce(Respond("fake_second_reauth_token", base::Minutes(30)));
  task_environment_.FastForwardBy(base::Seconds(10));
}

}  // namespace remoting::protocol
