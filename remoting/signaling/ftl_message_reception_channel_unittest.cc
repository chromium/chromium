// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_message_reception_channel.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "remoting/base/grpc_support/scoped_grpc_server_stream.h"
#include "remoting/base/grpc_test_support/grpc_test_util.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"
#include "remoting/signaling/ftl_grpc_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using ::testing::_;
using ::testing::Expectation;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::Return;

using ReceiveMessagesResponseCallback =
    base::RepeatingCallback<void(const ftl::ReceiveMessagesResponse&)>;
using StatusCallback = base::OnceCallback<void(const grpc::Status&)>;

// Fake stream implementation to allow probing if a stream is closed by client.
class FakeScopedGrpcServerStream : public ScopedGrpcServerStream {
 public:
  FakeScopedGrpcServerStream() : ScopedGrpcServerStream(nullptr) {}
  ~FakeScopedGrpcServerStream() override = default;

  base::WeakPtr<FakeScopedGrpcServerStream> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<FakeScopedGrpcServerStream> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(FakeScopedGrpcServerStream);
};

std::unique_ptr<FakeScopedGrpcServerStream> CreateFakeServerStream() {
  return std::make_unique<FakeScopedGrpcServerStream>();
}

// Creates a gmock EXPECT_CALL action that:
//   1. Creates a fake server stream and returns it as the start stream result
//   2. Posts a task to call |on_stream_opened| at the end of current sequence
//   3. Writes the WeakPtr to the fake server stream to |optional_out_stream|
//      if it is provided.
template <typename OnStreamOpenedLambda>
decltype(auto) StartStream(
    OnStreamOpenedLambda on_stream_opened,
    base::WeakPtr<FakeScopedGrpcServerStream>* optional_out_stream = nullptr) {
  return [=](base::OnceClosure on_channel_ready,
             const ReceiveMessagesResponseCallback& on_incoming_msg,
             StatusCallback on_channel_closed) {
    auto fake_stream = CreateFakeServerStream();
    if (optional_out_stream) {
      *optional_out_stream = fake_stream->GetWeakPtr();
    }
    auto on_stream_opened_cb = base::BindLambdaForTesting(on_stream_opened);
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(on_stream_opened_cb, std::move(on_channel_ready),
                       on_incoming_msg, std::move(on_channel_closed)));
    return fake_stream;
  };
}

base::OnceClosure NotReachedClosure() {
  return base::BindOnce([]() { NOTREACHED(); });
}

base::RepeatingCallback<void(const grpc::Status&)> NotReachedStatusCallback(
    const base::Location& location) {
  return base::BindLambdaForTesting([=](const grpc::Status& status) {
    NOTREACHED() << "Location: " << location.ToString()
                 << ", status code: " << status.error_code();
  });
}

}  // namespace

class FtlMessageReceptionChannelTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  base::TimeDelta GetTimeUntilRetry() const;
  int GetRetryFailureCount() const;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<FtlMessageReceptionChannel> channel_;
  base::MockCallback<FtlMessageReceptionChannel::StreamOpener>
      mock_stream_opener_;
  base::MockCallback<base::RepeatingCallback<void(const ftl::InboxMessage&)>>
      mock_on_incoming_msg_;
};

void FtlMessageReceptionChannelTest::SetUp() {
  channel_ = std::make_unique<FtlMessageReceptionChannel>();
  channel_->Initialize(mock_stream_opener_.Get(), mock_on_incoming_msg_.Get());
}

void FtlMessageReceptionChannelTest::TearDown() {
  channel_.reset();
  task_environment_.FastForwardUntilNoTasksRemain();
}

base::TimeDelta FtlMessageReceptionChannelTest::GetTimeUntilRetry() const {
  return channel_->GetReconnectRetryBackoffEntryForTesting()
      .GetTimeUntilRelease();
}

int FtlMessageReceptionChannelTest::GetRetryFailureCount() const {
  return channel_->GetReconnectRetryBackoffEntryForTesting().failure_count();
}

TEST_F(FtlMessageReceptionChannelTest,
       TestStartReceivingMessages_StoppedImmediately) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillOnce(StartStream(
          [&](base::OnceClosure on_channel_ready,
              const ReceiveMessagesResponseCallback& on_incoming_msg,
              StatusCallback on_channel_closed) {
            channel_->StopReceivingMessages();
          }));

  channel_->StartReceivingMessages(
      NotReachedClosure(),
      test::CheckStatusThenQuitRunLoopCallback(
          FROM_HERE, grpc::StatusCode::CANCELLED, &run_loop));

  run_loop.Run();
}

TEST_F(FtlMessageReceptionChannelTest,
       TestStartReceivingMessages_NotAuthenticated) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillOnce(StartStream(
          [&](base::OnceClosure on_channel_ready,
              const ReceiveMessagesResponseCallback& on_incoming_msg,
              StatusCallback on_channel_closed) {
            std::move(on_channel_closed)
                .Run(grpc::Status(grpc::StatusCode::UNAUTHENTICATED, ""));
          }));

  channel_->StartReceivingMessages(
      NotReachedClosure(),
      test::CheckStatusThenQuitRunLoopCallback(
          FROM_HERE, grpc::StatusCode::UNAUTHENTICATED, &run_loop));

  run_loop.Run();
}

TEST_F(FtlMessageReceptionChannelTest,
       TestStartReceivingMessages_StreamStarted) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillOnce(StartStream(
          [&](base::OnceClosure on_channel_ready,
              const ReceiveMessagesResponseCallback& on_incoming_msg,
              StatusCallback on_channel_closed) {
            std::move(on_channel_ready).Run();
          }));

  channel_->StartReceivingMessages(run_loop.QuitClosure(),
                                   NotReachedStatusCallback(FROM_HERE));

  run_loop.Run();
}

TEST_F(FtlMessageReceptionChannelTest,
       TestStartReceivingMessages_RecoverableStreamError) {
  base::RunLoop run_loop;

  base::WeakPtr<FakeScopedGrpcServerStream> old_stream;
  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillOnce(StartStream(
          [&](base::OnceClosure on_channel_ready,
              const ReceiveMessagesResponseCallback& on_incoming_msg,
              StatusCallback on_channel_closed) {
            // The first open stream attempt fails with UNAVAILABLE error.
            ASSERT_EQ(0, GetRetryFailureCount());

            std::move(on_channel_closed)
                .Run(grpc::Status(grpc::StatusCode::UNAVAILABLE, ""));

            ASSERT_EQ(1, GetRetryFailureCount());
            ASSERT_NEAR(FtlGrpcContext::kBackoffInitialDelay.InSecondsF(),
                        GetTimeUntilRetry().InSecondsF(), 0.5);

            // This will make the channel reopen the stream.
            task_environment_.FastForwardBy(GetTimeUntilRetry());
          },
          &old_stream))
      .WillOnce(StartStream(
          [&](base::OnceClosure on_channel_ready,
              const ReceiveMessagesResponseCallback& on_incoming_msg,
              StatusCallback on_channel_closed) {
            // Second open stream attempt succeeds.

            // Assert old stream closed.
            ASSERT_FALSE(old_stream);

            std::move(on_channel_ready).Run();

            ASSERT_EQ(0, GetRetryFailureCount());
          }));

  channel_->StartReceivingMessages(run_loop.QuitClosure(),
                                   NotReachedStatusCallback(FROM_HERE));

  run_loop.Run();
}

TEST_F(FtlMessageReceptionChannelTest,
       TestStartReceivingMessages_MultipleCalls) {
  base::RunLoop run_loop;

  base::MockCallback<base::OnceClosure> stream_ready_callback;

  // Exits the run loop iff the callback is called three times with OK.
  EXPECT_CALL(stream_ready_callback, Run())
      .WillOnce(Return())
      .WillOnce(Return())
      .WillOnce([&]() { run_loop.Quit(); });

  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillOnce(StartStream(
          [&](base::OnceClosure on_channel_ready,
              const ReceiveMessagesResponseCallback& on_incoming_msg,
              StatusCallback on_channel_closed) {
            std::move(on_channel_ready).Run();
          }));

  channel_->StartReceivingMessages(stream_ready_callback.Get(),
                                   NotReachedStatusCallback(FROM_HERE));
  channel_->StartReceivingMessages(stream_ready_callback.Get(),
                                   NotReachedStatusCallback(FROM_HERE));
  channel_->StartReceivingMessages(stream_ready_callback.Get(),
                                   NotReachedStatusCallback(FROM_HERE));

  run_loop.Run();
}

TEST_F(FtlMessageReceptionChannelTest, StreamsTwoMessages) {
  base::RunLoop run_loop;

  constexpr char kMessage1Id[] = "msg_1";
  constexpr char kMessage2Id[] = "msg_2";

  ftl::InboxMessage message_1;
  message_1.set_message_id(kMessage1Id);
  ftl::InboxMessage message_2;
  message_2.set_message_id(kMessage2Id);

  EXPECT_CALL(mock_on_incoming_msg_,
              Run(Property(&ftl::InboxMessage::message_id, kMessage1Id)))
      .WillOnce(Return());
  EXPECT_CALL(mock_on_incoming_msg_,
              Run(Property(&ftl::InboxMessage::message_id, kMessage2Id)))
      .WillOnce(Invoke([&](const ftl::InboxMessage&) { run_loop.Quit(); }));

  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillOnce(StartStream(
          [&](base::OnceClosure on_channel_ready,
              const ReceiveMessagesResponseCallback& on_incoming_msg,
              StatusCallback on_channel_closed) {
            std::move(on_channel_ready).Run();

            ftl::ReceiveMessagesResponse response;
            *response.mutable_inbox_message() = message_1;
            on_incoming_msg.Run(response);
            response.Clear();

            *response.mutable_inbox_message() = message_2;
            on_incoming_msg.Run(response);
            response.Clear();

            std::move(on_channel_closed).Run(grpc::Status::CANCELLED);
          }));

  channel_->StartReceivingMessages(
      base::DoNothing(),
      test::CheckStatusThenQuitRunLoopCallback(
          FROM_HERE, grpc::StatusCode::CANCELLED, &run_loop));

  run_loop.Run();
}

TEST_F(FtlMessageReceptionChannelTest, NoPongWithinTimeout_ResetsStream) {
  base::RunLoop run_loop;

  base::WeakPtr<FakeScopedGrpcServerStream> old_stream;
  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillOnce(StartStream(
          [&](base::OnceClosure on_channel_ready,
              const ReceiveMessagesResponseCallback& on_incoming_msg,
              StatusCallback on_channel_closed) {
            std::move(on_channel_ready).Run();
            task_environment_.FastForwardBy(
                FtlMessageReceptionChannel::kPongTimeout);

            ASSERT_EQ(1, GetRetryFailureCount());
            ASSERT_NEAR(FtlGrpcContext::kBackoffInitialDelay.InSecondsF(),
                        GetTimeUntilRetry().InSecondsF(), 0.5);

            // This will make the channel reopen the stream.
            task_environment_.FastForwardBy(GetTimeUntilRetry());
          },
          &old_stream))
      .WillOnce(StartStream(
          [&](base::OnceClosure on_channel_ready,
              const ReceiveMessagesResponseCallback& on_incoming_msg,
              StatusCallback on_channel_closed) {
            // Stream is reopened.

            // Assert old stream closed.
            ASSERT_FALSE(old_stream);

            std::move(on_channel_ready).Run();
            ASSERT_EQ(0, GetRetryFailureCount());
            run_loop.Quit();
          }));

  channel_->StartReceivingMessages(base::DoNothing(),
                                   NotReachedStatusCallback(FROM_HERE));

  run_loop.Run();
}

TEST_F(FtlMessageReceptionChannelTest, ServerClosesStream_ResetsStream) {
  base::RunLoop run_loop;

  base::WeakPtr<FakeScopedGrpcServerStream> old_stream;
  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillOnce(StartStream(
          [&](base::OnceClosure on_channel_ready,
              const ReceiveMessagesResponseCallback& on_incoming_msg,
              StatusCallback on_channel_closed) {
            auto fake_server_stream = CreateFakeServerStream();
            std::move(on_channel_ready).Run();

            // Close the stream with OK.
            std::move(on_channel_closed).Run(grpc::Status::OK);
          },
          &old_stream))
      .WillOnce(StartStream(
          [&](base::OnceClosure on_channel_ready,
              const ReceiveMessagesResponseCallback& on_incoming_msg,
              StatusCallback on_channel_closed) {
            ASSERT_FALSE(old_stream);

            std::move(on_channel_ready).Run();
            ASSERT_EQ(0, GetRetryFailureCount());
            run_loop.Quit();
          }));

  channel_->StartReceivingMessages(base::DoNothing(),
                                   NotReachedStatusCallback(FROM_HERE));

  run_loop.Run();
}

TEST_F(FtlMessageReceptionChannelTest, TimeoutIncreasesToMaximum) {
  base::RunLoop run_loop;

  int failure_count = 0;
  int hitting_max_delay_count = 0;
  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillRepeatedly(StartStream(
          [&](base::OnceClosure on_channel_ready,
              const ReceiveMessagesResponseCallback& on_incoming_msg,
              StatusCallback on_channel_closed) {
            // Quit if delay is ~kBackoffMaxDelay three times.
            if (hitting_max_delay_count == 3) {
              std::move(on_channel_ready).Run();
              ASSERT_EQ(0, GetRetryFailureCount());
              run_loop.Quit();
              return;
            }

            // Otherwise send UNAVAILABLE to reset the stream.

            std::move(on_channel_closed)
                .Run(grpc::Status(grpc::StatusCode::UNAVAILABLE, ""));

            int new_failure_count = GetRetryFailureCount();
            ASSERT_LT(failure_count, new_failure_count);
            failure_count = new_failure_count;

            base::TimeDelta time_until_retry = GetTimeUntilRetry();

            base::TimeDelta max_delay_diff =
                time_until_retry - FtlGrpcContext::kBackoffMaxDelay;

            // Adjust for fuzziness.
            if (max_delay_diff.magnitude() <
                base::TimeDelta::FromMilliseconds(500)) {
              hitting_max_delay_count++;
            }

            // This will tail-recursively call the stream opener.
            task_environment_.FastForwardBy(time_until_retry);
          }));

  channel_->StartReceivingMessages(base::DoNothing(),
                                   NotReachedStatusCallback(FROM_HERE));

  run_loop.Run();
}

TEST_F(FtlMessageReceptionChannelTest,
       StartStreamFailsWithUnRecoverableErrorAndRetry_TimeoutApplied) {
  base::RunLoop run_loop;

  base::WeakPtr<FakeScopedGrpcServerStream> old_stream;
  EXPECT_CALL(mock_stream_opener_, Run(_, _, _))
      .WillOnce(StartStream(
          [&](base::OnceClosure on_channel_ready,
              const ReceiveMessagesResponseCallback& on_incoming_msg,
              StatusCallback on_channel_closed) {
            // The first open stream attempt fails with UNAUTHENTICATED error.
            ASSERT_EQ(0, GetRetryFailureCount());

            std::move(on_channel_closed)
                .Run(grpc::Status(grpc::StatusCode::UNAUTHENTICATED, ""));

            ASSERT_EQ(1, GetRetryFailureCount());
            ASSERT_NEAR(FtlGrpcContext::kBackoffInitialDelay.InSecondsF(),
                        GetTimeUntilRetry().InSecondsF(), 0.5);
          },
          &old_stream))
      .WillOnce(StartStream(
          [&](base::OnceClosure on_channel_ready,
              const ReceiveMessagesResponseCallback& on_incoming_msg,
              StatusCallback on_channel_closed) {
            // Second open stream attempt succeeds.

            // Assert old stream closed.
            ASSERT_FALSE(old_stream);

            ASSERT_EQ(1, GetRetryFailureCount());

            std::move(on_channel_ready).Run();

            ASSERT_EQ(0, GetRetryFailureCount());
          }));

  channel_->StartReceivingMessages(
      base::DoNothing(),
      base::BindLambdaForTesting([&](const grpc::Status& status) {
        ASSERT_EQ(grpc::StatusCode::UNAUTHENTICATED, status.error_code());
        channel_->StartReceivingMessages(run_loop.QuitClosure(),
                                         NotReachedStatusCallback(FROM_HERE));
      }));

  run_loop.Run();
}

}  // namespace remoting
