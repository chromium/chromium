// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/corp_messaging_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "remoting/base/buildflags.h"
#include "remoting/base/certificate_helpers.h"
#include "remoting/base/http_status.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/base/protobuf_http_test_responder.h"
#include "remoting/proto/messaging_service.h"
#include "remoting/signaling/corp_message_channel_strategy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using ::testing::_;
using ::testing::Return;

using internal::HostSendMessageRequest;
using internal::HostSendMessageResponse;

constexpr char kFakePayload[] = "fake_payload";
constexpr char kFakeUsername[] = "fake_user";
constexpr char kFakeAuthzToken[] = "fake_token";
constexpr char kFakePublicKey[] = "fake_public_key";

using StatusCallback = CorpMessagingClient::StatusCallback;

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

}  // namespace

class CorpMessagingClientTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  ProtobufHttpTestResponder test_responder_;
  CorpMessagingClient messaging_client_{kFakeUsername, kFakePublicKey,
                                        test_responder_.GetUrlLoaderFactory(),
                                        CreateClientCertStoreInstance()};
};

TEST_F(CorpMessagingClientTest, TestSendMessage_Unauthenticated) {
  base::RunLoop run_loop;
  messaging_client_.SendMessage(
      kFakeAuthzToken, kFakePayload,
      CheckStatusThenQuitRunLoopCallback(
          FROM_HERE, HttpStatus::Code::UNAUTHENTICATED, &run_loop));
  test_responder_.AddErrorToMostRecentRequestUrl(
      HttpStatus(HttpStatus::Code::UNAUTHENTICATED, "Unauthenticated"));
  run_loop.Run();
}

TEST_F(CorpMessagingClientTest, TestSendMessage_SendOneMessage) {
  base::RunLoop run_loop;
  messaging_client_.SendMessage(
      kFakeAuthzToken, kFakePayload,
      CheckStatusThenQuitRunLoopCallback(FROM_HERE, HttpStatus::Code::OK,
                                         &run_loop));

  HostSendMessageRequest request;
  ASSERT_TRUE(test_responder_.GetMostRecentRequestMessage(&request));

  test_responder_.AddResponseToMostRecentRequestUrl(HostSendMessageResponse());
  run_loop.Run();
}

// Since the internals of the various messaging protos are not available in the
// public builds, complicated tests which rely on them are not worth running
// in that context.
#if BUILDFLAG(REMOTING_INTERNAL)

// TODO: joedow - Implement `StartReceivingMessages` tests after updating the
// internal helpers so we can generate server-side protos from the message
// structs.

#endif  // BUILDFLAG(REMOTING_INTERNAL)

}  // namespace remoting
