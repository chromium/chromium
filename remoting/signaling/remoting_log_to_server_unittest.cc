// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/remoting_log_to_server.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "remoting/base/fake_oauth_token_getter.h"
#include "remoting/proto/remoting/v1/telemetry_service.grpc.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using testing::_;

}  // namespace

class RemotingLogToServerTest : public testing::Test {
 public:
  RemotingLogToServerTest() {
    EXPECT_EQ(ServerLogEntry::ME2ME, log_to_server_.mode());
    log_to_server_.create_log_entry_ = mock_create_log_entry_.Get();
  }

  ~RemotingLogToServerTest() override {
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  const net::BackoffEntry& GetBackoffEntry() const {
    return log_to_server_.backoff_;
  }

  int GetMaxSendLogAttempts() const {
    return log_to_server_.kMaxSendLogAttempts;
  }

  using CreateLogEntryResponseCallback =
      RemotingLogToServer::CreateLogEntryResponseCallback;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::MockCallback<RemotingLogToServer::CreateLogEntryCallback>
      mock_create_log_entry_;

  RemotingLogToServer log_to_server_{
      ServerLogEntry::ME2ME,
      std::make_unique<FakeOAuthTokenGetter>(OAuthTokenGetter::SUCCESS,
                                             "fake_email",
                                             "fake_access_token")};
};

TEST_F(RemotingLogToServerTest, SuccessfullySendOneLog) {
  EXPECT_CALL(mock_create_log_entry_, Run(_, _))
      .WillOnce([&](const apis::v1::CreateLogEntryRequest& request,
                    CreateLogEntryResponseCallback callback) {
        ASSERT_EQ(1, request.payload().entry().field_size());
        ASSERT_EQ("test-key", request.payload().entry().field(0).key());
        ASSERT_EQ("test-value", request.payload().entry().field(0).value());
        std::move(callback).Run(grpc::Status::OK, {});
      });

  ServerLogEntry entry;
  entry.Set("test-key", "test-value");
  log_to_server_.Log(entry);

  task_environment_.FastForwardUntilNoTasksRemain();

  ASSERT_EQ(0, GetBackoffEntry().failure_count());
}

TEST_F(RemotingLogToServerTest, FailedToSend_RetryWithBackoff) {
  EXPECT_CALL(mock_create_log_entry_, Run(_, _))
      .Times(GetMaxSendLogAttempts())
      .WillRepeatedly([&](const apis::v1::CreateLogEntryRequest& request,
                          CreateLogEntryResponseCallback callback) {
        ASSERT_EQ(1, request.payload().entry().field_size());
        ASSERT_EQ("test-key", request.payload().entry().field(0).key());
        ASSERT_EQ("test-value", request.payload().entry().field(0).value());
        std::move(callback).Run(
            grpc::Status(grpc::StatusCode::UNAVAILABLE, "unavailable"), {});
      });

  ServerLogEntry entry;
  entry.Set("test-key", "test-value");
  log_to_server_.Log(entry);

  for (int i = 1; i <= GetMaxSendLogAttempts(); i++) {
    task_environment_.FastForwardBy(GetBackoffEntry().GetTimeUntilRelease());
    ASSERT_EQ(i, GetBackoffEntry().failure_count());
  }
}

TEST_F(RemotingLogToServerTest, FailedToSendTwoLogs_RetryThenSucceeds) {
  CreateLogEntryResponseCallback response_callback_1;
  CreateLogEntryResponseCallback response_callback_2;
  EXPECT_CALL(mock_create_log_entry_, Run(_, _))
      .WillOnce([&](const apis::v1::CreateLogEntryRequest& request,
                    CreateLogEntryResponseCallback callback) {
        ASSERT_EQ(1, request.payload().entry().field_size());
        ASSERT_EQ("test-key-1", request.payload().entry().field(0).key());
        ASSERT_EQ("test-value-1", request.payload().entry().field(0).value());
        response_callback_1 = std::move(callback);
      })
      .WillOnce([&](const apis::v1::CreateLogEntryRequest& request,
                    CreateLogEntryResponseCallback callback) {
        ASSERT_EQ(1, request.payload().entry().field_size());
        ASSERT_EQ("test-key-2", request.payload().entry().field(0).key());
        ASSERT_EQ("test-value-2", request.payload().entry().field(0).value());
        response_callback_2 = std::move(callback);
      })
      .WillOnce([&](const apis::v1::CreateLogEntryRequest& request,
                    CreateLogEntryResponseCallback callback) {
        ASSERT_EQ(1, request.payload().entry().field_size());
        ASSERT_EQ("test-key-1", request.payload().entry().field(0).key());
        ASSERT_EQ("test-value-1", request.payload().entry().field(0).value());
        response_callback_1 = std::move(callback);
      })
      .WillOnce([&](const apis::v1::CreateLogEntryRequest& request,
                    CreateLogEntryResponseCallback callback) {
        ASSERT_EQ(1, request.payload().entry().field_size());
        ASSERT_EQ("test-key-2", request.payload().entry().field(0).key());
        ASSERT_EQ("test-value-2", request.payload().entry().field(0).value());
        response_callback_2 = std::move(callback);
      });

  ServerLogEntry entry_1;
  entry_1.Set("test-key-1", "test-value-1");
  log_to_server_.Log(entry_1);
  task_environment_.FastForwardUntilNoTasksRemain();

  ServerLogEntry entry_2;
  entry_2.Set("test-key-2", "test-value-2");
  log_to_server_.Log(entry_2);
  task_environment_.FastForwardUntilNoTasksRemain();

  ASSERT_EQ(0, GetBackoffEntry().failure_count());

  std::move(response_callback_1)
      .Run(grpc::Status(grpc::StatusCode::UNAVAILABLE, "unavailable"), {});
  task_environment_.FastForwardUntilNoTasksRemain();
  std::move(response_callback_2)
      .Run(grpc::Status(grpc::StatusCode::UNAVAILABLE, "unavailable"), {});
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_EQ(2, GetBackoffEntry().failure_count());

  std::move(response_callback_1).Run(grpc::Status::OK, {});
  std::move(response_callback_2).Run(grpc::Status::OK, {});
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_EQ(0, GetBackoffEntry().failure_count());
}

}  // namespace remoting
