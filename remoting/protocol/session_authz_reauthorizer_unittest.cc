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
#include "remoting/base/mock_session_authz_service_client.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/proto/session_authz_service.h"
#include "remoting/protocol/errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::protocol {
namespace {

using testing::_;

constexpr char kSessionId[] = "fake_session_id";
constexpr char kInitialReauthToken[] = "fake_initial_reauth_token";
constexpr base::TimeDelta kInitialTokenLifetime = base::Minutes(10);

internal::ReauthorizeHostRequestStruct CreateRequest(
    std::string_view session_id,
    std::string_view session_reauth_token) {
  internal::ReauthorizeHostRequestStruct request;
  request.session_id = session_id;
  request.session_reauth_token = session_reauth_token;
  return request;
}

auto Respond(std::string_view session_reauth_token,
             base::TimeDelta session_reauth_token_lifetime) {
  auto response = std::make_unique<internal::ReauthorizeHostResponseStruct>();
  response->session_reauth_token = session_reauth_token;
  response->session_reauth_token_lifetime = session_reauth_token_lifetime;
  return base::test::RunOnceCallback<1>(ProtobufHttpStatus::OK(),
                                        std::move(response));
}

template <typename CodeType>
auto RespondError(CodeType code) {
  return base::test::RunOnceCallbackRepeatedly<1>(ProtobufHttpStatus(code),
                                                  nullptr);
}

}  // namespace

class SessionAuthzReauthorizerTest : public testing::Test {
 public:
  SessionAuthzReauthorizerTest();
  ~SessionAuthzReauthorizerTest() override;

 protected:
  auto ResetReauthorizer();

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::MockCallback<base::OnceClosure> on_reauthorization_failed_callback_;
  std::unique_ptr<SessionAuthzReauthorizer> reauthorizer_;
  MockSessionAuthzServiceClient service_client_;
};

SessionAuthzReauthorizerTest::SessionAuthzReauthorizerTest() {
  reauthorizer_ = std::make_unique<SessionAuthzReauthorizer>(
      &service_client_, kSessionId, kInitialReauthToken, kInitialTokenLifetime,
      on_reauthorization_failed_callback_.Get());
  reauthorizer_->Start();
}

SessionAuthzReauthorizerTest::~SessionAuthzReauthorizerTest() = default;

auto SessionAuthzReauthorizerTest::ResetReauthorizer() {
  return [&]() { reauthorizer_.reset(); };
}

TEST_F(SessionAuthzReauthorizerTest, MultipleSuccessfulReauths) {
  // Reauth is not triggered before half of the token lifetime has passed.
  EXPECT_CALL(service_client_, ReauthorizeHost(_, _)).Times(0);
  task_environment_.FastForwardBy(kInitialTokenLifetime / 2 -
                                  base::Seconds(10));

  // Reauth is triggered now.
  EXPECT_CALL(
      service_client_,
      ReauthorizeHost(CreateRequest(kSessionId, kInitialReauthToken), _))
      .WillOnce(Respond("fake_second_reauth_token", base::Minutes(8)));
  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_CALL(service_client_, ReauthorizeHost(_, _)).Times(0);
  task_environment_.FastForwardBy(base::Minutes(4) - base::Seconds(10));

  EXPECT_CALL(
      service_client_,
      ReauthorizeHost(CreateRequest(kSessionId, "fake_second_reauth_token"), _))
      .WillOnce(Respond("fake_third_reauth_token", base::Minutes(6)));
  task_environment_.FastForwardBy(base::Seconds(10));
}

TEST_F(SessionAuthzReauthorizerTest,
       ReauthFailedWithNonretriableError_ClosesSession) {
  EXPECT_CALL(
      service_client_,
      ReauthorizeHost(CreateRequest(kSessionId, kInitialReauthToken), _))
      .WillOnce(RespondError(net::HTTP_FORBIDDEN));
  EXPECT_CALL(on_reauthorization_failed_callback_, Run())
      .WillOnce(ResetReauthorizer());
  task_environment_.FastForwardBy(kInitialTokenLifetime / 2);
}

TEST_F(SessionAuthzReauthorizerTest, RetryReauthAndSucceed) {
  EXPECT_CALL(
      service_client_,
      ReauthorizeHost(CreateRequest(kSessionId, kInitialReauthToken), _))
      .WillRepeatedly(RespondError(net::ERR_INTERNET_DISCONNECTED));
  task_environment_.FastForwardBy(kInitialTokenLifetime / 2);
  const net::BackoffEntry* backoff_entry =
      reauthorizer_->GetBackoffEntryForTest();
  task_environment_.FastForwardBy(backoff_entry->GetTimeUntilRelease());
  task_environment_.FastForwardBy(backoff_entry->GetTimeUntilRelease());

  EXPECT_CALL(
      service_client_,
      ReauthorizeHost(CreateRequest(kSessionId, kInitialReauthToken), _))
      .WillOnce(Respond("fake_second_reauth_token", base::Minutes(8)));
  task_environment_.FastForwardBy(backoff_entry->GetTimeUntilRelease());

  EXPECT_CALL(
      service_client_,
      ReauthorizeHost(CreateRequest(kSessionId, "fake_second_reauth_token"), _))
      .WillOnce(Respond("fake_third_reauth_token", base::Minutes(6)));
  task_environment_.FastForwardBy(base::Minutes(4));
}

TEST_F(SessionAuthzReauthorizerTest, RetriesExceedTokenLifetime_ClosesSession) {
  EXPECT_CALL(
      service_client_,
      ReauthorizeHost(CreateRequest(kSessionId, kInitialReauthToken), _))
      .WillRepeatedly(RespondError(net::ERR_INTERNET_DISCONNECTED));
  EXPECT_CALL(on_reauthorization_failed_callback_, Run()).Times(0);
  task_environment_.FastForwardBy(kInitialTokenLifetime / 2);
  const net::BackoffEntry* backoff_entry =
      reauthorizer_->GetBackoffEntryForTest();
  task_environment_.FastForwardBy(backoff_entry->GetTimeUntilRelease());
  task_environment_.FastForwardBy(backoff_entry->GetTimeUntilRelease());

  EXPECT_CALL(on_reauthorization_failed_callback_, Run())
      .WillOnce(ResetReauthorizer());
  task_environment_.FastForwardBy(kInitialTokenLifetime / 2);
}

}  // namespace remoting::protocol
